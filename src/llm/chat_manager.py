import os
from typing import List, Dict, Optional
from langchain_anthropic import ChatAnthropic
from langchain.schema import HumanMessage, AIMessage
from langchain.callbacks.manager import tracing_v2_enabled
from langsmith import Client

class ChatManager:
    def __init__(self):
        # Ensure environment is set up
        self._verify_environment()
        
        # Initialize LangSmith client
        self.langsmith_client = Client()
        
        # Initialize the model
        self.llm = ChatAnthropic(
            model=os.getenv("AI_MODEL", "claude-3-opus-20240229"),
            anthropic_api_key=os.getenv("ANTHROPIC_API_KEY"),
            temperature=float(os.getenv("TEMPERATURE", "0.7")),
            max_tokens=int(os.getenv("MAX_TOKENS", "4096")),
        )
        
        # Store active chat sessions
        self._chat_sessions: Dict[str, List[Dict[str, str]]] = {}
        
    def _verify_environment(self):
        """Verify all required environment variables are set."""
        required_vars = [
            "ANTHROPIC_API_KEY",
            "LANGCHAIN_API_KEY",
            "LANGCHAIN_PROJECT",
            "LANGCHAIN_TRACING_V2",
            "LANGCHAIN_ENDPOINT"
        ]
        missing = [var for var in required_vars if not os.getenv(var)]
        if missing:
            raise EnvironmentError(f"Missing required environment variables: {missing}")
    
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
        # Get or initialize chat history
        if history is not None:
            self._chat_sessions[chat_id] = history
        elif chat_id not in self._chat_sessions:
            self._chat_sessions[chat_id] = []
        
        current_history = self._chat_sessions[chat_id]
        
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
        with tracing_v2_enabled(project_name=os.getenv("LANGCHAIN_PROJECT", "aguila-project")):
            response = self.llm.invoke(messages)
        
        # Update chat history
        self._chat_sessions[chat_id].extend([
            {"role": "human", "content": message},
            {"role": "assistant", "content": response.content}
        ])
        
        return response.content
    
    def get_chat_history(self, chat_id: str) -> List[Dict[str, str]]:
        """Get the message history for a specific chat session."""
        return self._chat_sessions.get(chat_id, [])
    
    def clear_chat_history(self, chat_id: str) -> None:
        """Clear the message history for a specific chat session."""
        self._chat_sessions.pop(chat_id, None) 