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
            2. Identify if it's a direct command ("tune to 145 MHz") or indirect request ("show me air traffic")
            3. Provide your reasoning and confidence level
            
            IMPORTANT RULES:
            - Only return requires_tuning=true if you have a SPECIFIC frequency to tune to
            - For indirect or ambiguous requests, return requires_tuning=false and explain what additional information is needed
            - Do NOT use tools unless absolutely necessary
            - If you need more information from the user, simply include it in the NEEDS_INFO field
            - Never try to tune to a frequency without explicit user confirmation
            
            Common SDR-related requests include:
            - Direct frequency tuning ("tune to 145.5 MHz")
            - Band exploration ("check the FM band")
            - Signal hunting ("find some air traffic")
            - Mode changes ("switch to AM mode")
            - Spectrum analysis ("show me the waterfall around 450 MHz")
            
            Available tools: {tools}
            Tool names: {tool_names}
            
            You must output your response in this exact format:
            REQUIRES_TUNING: [true/false]
            CONFIDENCE: [high/medium/low]
            REASONING: [your detailed reasoning]
            FREQUENCY_MENTIONED: [specific frequency if mentioned, "none" if not]
            NEEDS_INFO: [what additional information is needed from the user, if any]
            
            {agent_scratchpad}
            """),
            ("human", "{input}"),
            ("assistant", "Let me analyze that request.")
        ])

    def _parse_frequency(self, text: str) -> Optional[int]:
        """Extract frequency in Hz from text"""
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
            # Get frequency from message
            freq = self._parse_frequency(message)
            
            if freq:
                # Convert to MHz for display
                freq_mhz = freq / 1_000_000
                
                # Do the actual tuning
                self.tuning_tool.run({"frequency": freq})
                
                # Just return a simple success message
                return {
                    "requires_tuning": "true", 
                    "frequency_mentioned": str(freq),
                    "confidence": "high",
                    "tuning_result": f"I've tuned the radio to {freq_mhz:.3f} MHz for you.",
                    "success": True
                }
            
            # Not a tuning request
            return {
                "requires_tuning": "false",
                "frequency_mentioned": "none",
                "confidence": "low",
                "tuning_result": "",
                "success": True
            }
                
        except Exception as e:
            return {
                "requires_tuning": "false",
                "confidence": "low",
                "tuning_result": str(e),
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