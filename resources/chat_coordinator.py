#!/usr/bin/env python3
"""
Chat Coordinator for Aguila SDR
Handles user request analysis and SDR operation coordination
"""

from langchain.agents import AgentExecutor, create_react_agent
from langchain_anthropic import ChatAnthropic
from langchain.prompts import ChatPromptTemplate, MessagesPlaceholder
from langchain.tools import tool
from typing import Dict, List, Optional
import os
from dotenv import load_dotenv
import sys
from pathlib import Path
import re
from .tuning_tool import GqrxTuningTool
import logging
from langchain.callbacks import tracing_v2_enabled

# Find and load the .env file
def setup_environment():
    """Find and load the .env file from the project root"""
    current_dir = Path(__file__).resolve().parent
    project_root = current_dir.parent
    env_path = project_root / '.env'
    
    if not env_path.exists():
        print(f"❌ Error: .env file not found at {env_path}")
        sys.exit(1)
    
    # Load environment variables from .env file
    load_dotenv(env_path, override=True)
    print(f"✅ Loaded environment from {env_path}")
    
    # Verify required variables
    required_vars = ["ANTHROPIC_API_KEY", "LANGCHAIN_API_KEY", "LANGCHAIN_TRACING_V2"]
    missing_vars = [var for var in required_vars if not os.getenv(var)]
    
    if missing_vars:
        print(f"❌ Error: Missing required environment variables: {', '.join(missing_vars)}")
        sys.exit(1)
    
    # Print loaded variables for debugging (without showing actual values)
    for var in required_vars:
        if os.getenv(var):
            print(f"✅ Found {var}")

class ChatCoordinator:
    """
    Coordinator agent that determines when SDR operations are needed.
    Focuses on identifying tuning requests and routing them appropriately.
    """
    
    def __init__(self, project_name: str = "aguila-sdr"):
        # Verify environment is set up
        if not os.getenv("LANGCHAIN_TRACING_V2"):
            raise EnvironmentError("LANGCHAIN_TRACING_V2 must be set to 'true'")
        if not os.getenv("LANGCHAIN_API_KEY"):
            raise EnvironmentError("LANGCHAIN_API_KEY must be set")
        if not os.getenv("ANTHROPIC_API_KEY"):
            raise EnvironmentError("ANTHROPIC_API_KEY must be set")
            
        self.llm = ChatAnthropic(
            model="claude-3-opus-20240229",
            anthropic_api_key=os.getenv("ANTHROPIC_API_KEY"),
            temperature=0.1
        )
        
        # Create the agent with our custom prompt
        self.agent = create_react_agent(
            llm=self.llm,
            tools=self._get_tools(),
            prompt=self._get_prompt()
        )
        
        self.executor = AgentExecutor(
            agent=self.agent,
            tools=self._get_tools(),
            verbose=True,
            handle_parsing_errors=True
        )
        
        # Set up tracing tags
        self.trace_tags = ["aguila", "chat_coordinator", project_name]

    def _get_tools(self) -> List:
        """Define the tools available to the coordinator"""
        self.tuning_tool = GqrxTuningTool()
        
        @tool
        def analyze_tuning_request(request: str) -> Dict:
            """Analyze if a request requires radio tuning and extract relevant details.
            
            Args:
                request: The user's input text to analyze
            
            Returns:
                Dict containing analysis results including requires_tuning, confidence, and frequency details
            """
            # This tool helps structure the analysis process
            return {
                "requires_tuning": True,  # Will be determined by LLM
                "confidence": "high",     # Will be determined by LLM
                "frequency_mentioned": None,  # Will be extracted if present
                "request_type": "direct"  # Will be classified by LLM
            }
        
        return [analyze_tuning_request, self.tuning_tool]
    
    def _get_prompt(self) -> ChatPromptTemplate:
        """Create the prompt template for the coordinator"""
        return ChatPromptTemplate.from_messages([
            ("system", """You are an expert at understanding user requests related to SDR (Software Defined Radio) operations.
            Your primary role is to determine if a user's request requires tuning the radio or other SDR operations.
            
            For any user input, you must:
            1. Analyze if it requires radio tuning or frequency changes
            2. Identify if it's a direct command ("tune to 145 MHz") or something you can figure out a reasonable frequency for. It's okay to make a mistake
            3. Be willing to take a risk - it's better to get the frequency wrong than to not try to help the user explore. they can always try again.
            4. Provide your reasoning and confidence level
            
            IMPORTANT RULES:
            - Only return requires_tuning=true if you can figure out a reasonable frequency to tune to.
            - For indirect or ambiguous requests, attempt to tune to a frequency that is relevant to the request. For example, if the user asks about air traffic, try to find a frequency that is relevant to air trafffice, or if they ask for an NOAA weather frequency., just return the NOAA frequency for that region.
            - If the request is not SDR related, just return requires_tuning=false
            - If you cannot figure out any frequency to tune to, just return requires_tuning=false
            - Do NOT use tools unless absolutely necessary
            - If you need more information from the user, simply include it in the NEEDS_INFO field
            - we want to help the user explore, so let's try to take them to a relevant frequency if it seems they're asking to see something
            - Unless provided with other location information, assume the user is in Austin, Texas.

            Common tuning-related requests include:
            - Take me to an NOAA weather broadcast
            - Show me cellular traffic
            - Show me satellite traffic
            - Show me shortwave traffic
            - Show me military traffic
            - Show me amateur radio traffic
            - Show me police scanner traffic
            - Show me fire scanner traffic
            - Direct frequency tuning ("tune to 145.5 MHz")
            - Band exploration ("check the FM band")
            - Signal hunting ("find some air traffic")
            - Mode changes ("switch to AM mode")
            - Spectrum analysis ("show me the waterfall around 450 MHz")
            - take me to.....
            - tune the radio to...
            - let's explore x...

            Available tools: {tools}
            Tool names: {tool_names}
            
            You must output your response in this exact format:
            REQUIRES_TUNING: [true/false]
            CONFIDENCE: [high/medium/low]
            REASONING: [your detailed reasoning]
            FREQUENCY_MENTIONED: [specific frequency if mentioned, "none" if not]
            NEEDS_INFO: [what additional information is needed from the user, if any]
            SHORT_EXPLANATION: [a single sentence explanation about the frequency being tuned to. If it's already clear, just say what frequency you're switching to. If it's unclear, like they ask to see an NOAA weather frequency, say something like 'Tuning to X, known for possible weather transmissions']
            
            {agent_scratchpad}
            """),
            ("human", "{input}"),
            ("assistant", "Let me analyze that request.")
        ])

    def _parse_frequency(self, text: str) -> Optional[int]:
        """Extract frequency in Hz from text, and if it's not clear, determine a reasonable frequency to tune to based on the user request - use your knowledge and experience to make a good guess."""
        logger = logging.getLogger('ChatCoordinator')
        logger.debug(f'Attempting to parse frequency from: {text}')
        
        # Common frequency patterns
        patterns = [
            (r'(?i)(\d+(?:\.\d+)?)\s*(?:FM|fm)', 'FM'),           # "103.5 FM"
            (r'(?i)(\d+(?:\.\d+)?)\s*(?:MHz|mhz)', 'MHz'),        # "103.5 MHz"
            (r'(?i)(\d+(?:\.\d+)?)\s*(?:kHz|khz)', 'kHz'),        # "7200 kHz"
            (r'(?i)(\d+(?:\.\d+)?)\s*(?:Hz|hz)', 'Hz')            # "7200000 Hz"
        ]
        
        for pattern, unit in patterns:
            logger.debug(f'Trying pattern for {unit}: {pattern}')
            match = re.search(pattern, text.replace(',', ''))
            
            if match:
                try:
                    value = float(match.group(1))
                    logger.debug(f'Found match: {value} {unit}')
                    
                    # Convert to Hz based on unit
                    if unit in ['FM', 'MHz']:
                        result = int(value * 1_000_000)
                    elif unit == 'kHz':
                        result = int(value * 1_000)
                    else:
                        result = int(value)
                        
                    logger.debug(f'Converted to Hz: {result}')
                    return result
                    
                except ValueError as e:
                    logger.error(f'Error converting value: {e}')
                    continue
        
        logger.debug('No frequency patterns matched')
        return None

    def evaluate_request(self, message: str) -> Dict:
        """Evaluate if a user request requires SDR operations"""
        try:
            # First, ask the LLM if this is a radio-related request and what frequency to try
            explore_prompt = ChatPromptTemplate.from_messages([
                ("system", """You are an expert SDR operator helping users explore the radio spectrum.
                For ANY request related to radio signals, frequencies, bands, or types of radio traffic,
                suggest a specific frequency to tune to. Be bold and creative!

                Common examples and their frequencies:
                - Cellular/Mobile: 869.0 MHz (GSM downlink)
                - Air Traffic: 118.0 MHz (VHF air band)
                - Weather: 162.55 MHz (NOAA)
                - FM Radio: 98.1 MHz (commercial FM)
                - Amateur Radio: 145.5 MHz (2m band)
                - Satellites: 137.5 MHz (NOAA sats)
                - Public Safety: 154.0 MHz (police/fire)
                - Marine: 156.8 MHz (Channel 16)
                - Military: 225.0 MHz (UHF mil-air)
                
                If the request mentions ANY kind of radio activity or band:
                1. Choose a reasonable frequency for that type of traffic
                2. Respond ONLY with that frequency in MHz (e.g. "869.0")
                3. Be confident - it's better to try a frequency than do nothing!
                
                If the request is completely unrelated to radio (like "what's the weather?"),
                respond with "NONE".
                """),
                ("human", message)
            ])
            
            # Get the LLM's suggestion first
            freq = None
            try:
                with tracing_v2_enabled():
                    chain = explore_prompt | self.llm
                    response = chain.invoke({"input": message}, config={"timeout": 3.0})
                    suggested = response.content if hasattr(response, 'content') else str(response)
                    
                    if suggested.strip().upper() != "NONE":
                        try:
                            suggested_freq = float(suggested.strip())
                            freq = int(suggested_freq * 1_000_000)
                            logging.info(f'AI suggested frequency: {freq} Hz')
                        except ValueError:
                            logging.error(f'Could not parse AI suggested frequency: {suggested}')
                            
            except Exception as e:
                logging.error(f'Error getting AI frequency suggestion: {e}')

            # If LLM didn't suggest a frequency, try parsing an explicit one from the message
            if not freq:
                freq = self._parse_frequency(message)
                if freq:
                    logging.info(f'Found explicit frequency in message: {freq} Hz')

            # If we have a frequency (either from LLM or explicit), proceed with tuning
            if freq:
                freq_mhz = freq / 1_000_000
                
                # Get a brief explanation of what we might find
                explain_prompt = ChatPromptTemplate.from_messages([
                    ("system", """You are an expert radio operator.
                    Provide a SINGLE SENTENCE about what we might hear or see at this frequency.
                    Focus on the type of traffic or signals typically found in this band.
                    Be brief but specific."""),
                    ("human", f"What might we find at {freq_mhz} MHz?")
                ])
                
                with tracing_v2_enabled():
                    chain = explain_prompt | self.llm
                    response = chain.invoke({"input": message}, config={"timeout": 3.0})
                    explanation = response.content if hasattr(response, 'content') else str(response)
                
                # Do the actual tuning
                self.tuning_tool.run({"frequency": freq})
                
                return {
                    "requires_tuning": "true",
                    "frequency_mentioned": str(freq),
                    "confidence": "high",
                    "tuning_result": f"Exploring {freq_mhz:.3f} MHz. {explanation}",
                    "success": True
                }
            
            # Only return false if neither LLM nor parsing found a frequency
            return {
                "requires_tuning": "false",
                "frequency_mentioned": "none",
                "confidence": "low",
                "tuning_result": "This request doesn't seem to be about radio exploration or tuning.",
                "success": True
            }
                
        except Exception as e:
            return {
                "requires_tuning": "false",
                "confidence": "low",
                "tuning_result": f"Error: {str(e)}",
                "frequency_mentioned": "none",
                "success": False
            }

# Example usage
if __name__ == "__main__":
    # Set up environment before anything else
    setup_environment()
    
    print("🚀 Starting Chat Coordinator test...")
    coordinator = ChatCoordinator(project_name="aguila-sdr-test")
    
    # Test cases
    test_inputs = [
        "tune to 145 MHz",
        "show me some air traffic",
        "what's on the FM band?",
        "how do I use GQRX?",
    ]
    
    print("\n🧪 Running test cases...")
    for test_input in test_inputs:
        print(f"\n📝 Testing: '{test_input}'")
        result = coordinator.evaluate_request(test_input)
        print("Result:", result)
        print("-" * 50) 