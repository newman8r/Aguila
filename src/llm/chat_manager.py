import os
from typing import List, Dict, Optional
from langchain_anthropic import ChatAnthropic
from langchain.schema import HumanMessage, AIMessage
from langchain.callbacks.manager import tracing_v2_enabled
from langsmith import Client
import logging

# Configure logging
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger("ChatManager")

# Import our coordinator - it's in resources/
from resources.chat_coordinator import ChatCoordinator

class ChatManager:
    def __init__(self):
        logger.info("Initializing ChatManager...")
        
        # Log environment configuration
        logger.info("Using environment configuration...")
        logger.info(f"✓ Using ANTHROPIC_API_KEY: {'set' if os.getenv('ANTHROPIC_API_KEY') else 'not set'}")
        logger.info(f"✓ LangChain project: {os.getenv('LANGCHAIN_PROJECT', 'default')}")
        
        # Initialize LangSmith client if credentials available
        if os.getenv("LANGCHAIN_API_KEY"):
            self.langsmith_client = Client()
        
        # Initialize the model with fallback values if needed
        self.llm = ChatAnthropic(
            model=os.getenv("AI_MODEL", "claude-3-opus-20240229"),
            anthropic_api_key=os.getenv("ANTHROPIC_API_KEY", "default_key"),
            temperature=float(os.getenv("TEMPERATURE", "0.7")),
            max_tokens=int(os.getenv("MAX_TOKENS", "4096")),
        )
        
        # Initialize the chat coordinator
        logger.info("Initializing ChatCoordinator...")
        try:
            self.coordinator = ChatCoordinator()
            logger.info("ChatCoordinator initialized successfully")
        except Exception as e:
            logger.warning(f"ChatCoordinator initialization failed: {str(e)}")
            self.coordinator = None
        
        # Store active chat sessions
        self._chat_sessions: Dict[str, List[Dict[str, str]]] = {}
        logger.info("ChatManager initialization complete")
        
    def chat(self, message: str, chat_id: str, history: Optional[List[Dict[str, str]]] = None) -> str:
        """
        Send a chat message and get a response for a specific chat session.
        
        Args:
            message: The user's message
            chat_id: Unique identifier for this chat session
            history: Optional list of previous messages. If not provided, will use stored history
                    Format: [{"role": "human", "content": "..."}, {"role": "assistant", "content": "..."}]
        
        Returns:
            The assistant's response
        """
        logger.info(f"Processing chat message for chat_id {chat_id}")
        logger.debug(f"Message content: {message}")
        
        # First, analyze the message with the coordinator
        logger.info("Analyzing message with ChatCoordinator...")
        try:
            analysis = self.coordinator.evaluate_request(message)
            logger.info(f"Coordinator analysis: {analysis}")
        except Exception as e:
            logger.error(f"Error during coordinator analysis: {str(e)}")
            analysis = {"requires_tuning": "false", "error": str(e)}
        
        # If tuning is required and we have a specific frequency
        if analysis.get("requires_tuning") == "true" and "tuning_result" in analysis:
            logger.info("Tuning request detected, executing tuning operation")
            response = analysis["tuning_result"]
            logger.info(f"Tuning result: {response}")
            
        # If we need more information
        elif analysis.get("needs_info"):
            logger.info("More information needed from user")
            response = analysis["needs_info"]
            logger.info(f"Requesting info: {response}")
            
        # For all other messages, use the normal chat flow
        else:
            logger.info("Using normal chat flow with Claude")
            # Get or initialize chat history
            if history is not None:
                self._chat_sessions[chat_id] = history
            elif chat_id not in self._chat_sessions:
                self._chat_sessions[chat_id] = []
            
            current_history = self._chat_sessions[chat_id]
            logger.debug(f"Current history length: {len(current_history)}")
            
            # Convert history to LangChain message format
            messages = []
            for msg in current_history:
                if msg["role"] == "human":
                    messages.append(HumanMessage(content=msg["content"]))
                else:
                    messages.append(AIMessage(content=msg["content"]))
            
            # Add current message
            messages.append(HumanMessage(content=message))
            
            # Get response with tracing enabled
            logger.info("Sending request to Claude...")
            with tracing_v2_enabled(project_name=os.getenv("LANGCHAIN_PROJECT", "aguila-project")):
                response = self.llm.invoke(messages)
                response = response.content
            logger.info("Received response from Claude")
            logger.debug(f"Claude response: {response}")
        
        # Update chat history
        logger.info("Updating chat history")
        self._chat_sessions[chat_id].extend([
            {"role": "human", "content": message},
            {"role": "assistant", "content": response}
        ])
        
        return response
    
    def get_chat_history(self, chat_id: str) -> List[Dict[str, str]]:
        """Get the message history for a specific chat session."""
        return self._chat_sessions.get(chat_id, [])
    
    def clear_chat_history(self, chat_id: str) -> None:
        """Clear the message history for a specific chat session."""
        self._chat_sessions.pop(chat_id, None)

# Test the chat manager if run directly
if __name__ == "__main__":
    logger.info("Running ChatManager test...")
    
    # Initialize chat manager
    manager = ChatManager()
    
    # Test messages
    test_messages = [
        "tune to 145 MHz",
        "show me some air traffic",
        "what's on the FM band?",
        "how do I use GQRX?"
    ]
    
    # Run tests
    for msg in test_messages:
        logger.info(f"\nTesting message: {msg}")
        response = manager.chat(msg, "test_session")
        logger.info(f"Response: {response}")
        logger.info("-" * 50) 