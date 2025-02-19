import os
import pytest
from dotenv import load_dotenv
from src.llm.chat_manager import ChatManager

# Load environment variables from .env file
load_dotenv()

def test_chat_manager_initialization():
    """Test that ChatManager initializes correctly with environment variables."""
    chat_manager = ChatManager()
    assert chat_manager.llm is not None

def test_simple_chat():
    """Test a simple chat interaction with tracing."""
    chat_manager = ChatManager()
    chat_id = "test_chat_1"
    response = chat_manager.chat("Hello! How are you?", chat_id=chat_id)
    assert isinstance(response, str)
    assert len(response) > 0
    
    # Verify history was stored
    history = chat_manager.get_chat_history(chat_id)
    assert len(history) == 2  # One human message, one assistant message
    assert history[0]["role"] == "human"
    assert history[1]["role"] == "assistant"

def test_chat_with_history():
    """Test chat with history."""
    chat_manager = ChatManager()
    chat_id = "test_chat_2"
    
    # Initialize with history
    history = [
        {"role": "human", "content": "What is your name?"},
        {"role": "assistant", "content": "I am Claude, an AI assistant."},
        {"role": "human", "content": "What did I just ask you?"},
    ]
    response = chat_manager.chat(
        "Can you remind me what we discussed?", 
        chat_id=chat_id,
        history=history
    )
    assert isinstance(response, str)
    assert len(response) > 0
    assert "name" in response.lower()
    
    # Verify history was updated
    updated_history = chat_manager.get_chat_history(chat_id)
    assert len(updated_history) == 5  # 3 initial messages + new Q&A pair

def test_multiple_chat_sessions():
    """Test that different chat sessions maintain separate histories."""
    chat_manager = ChatManager()
    chat_id_1 = "session_1"
    chat_id_2 = "session_2"
    
    # Chat session 1
    chat_manager.chat("Hello from session 1", chat_id=chat_id_1)
    
    # Chat session 2
    chat_manager.chat("Hello from session 2", chat_id=chat_id_2)
    
    # Verify separate histories
    history_1 = chat_manager.get_chat_history(chat_id_1)
    history_2 = chat_manager.get_chat_history(chat_id_2)
    
    assert "session 1" in history_1[0]["content"]
    assert "session 2" in history_2[0]["content"]
    assert len(history_1) == 2  # One Q&A pair
    assert len(history_2) == 2  # One Q&A pair

def test_clear_chat_history():
    """Test clearing chat history."""
    chat_manager = ChatManager()
    chat_id = "test_chat_3"
    
    # Add some messages
    chat_manager.chat("Hello!", chat_id=chat_id)
    assert len(chat_manager.get_chat_history(chat_id)) > 0
    
    # Clear history
    chat_manager.clear_chat_history(chat_id)
    assert len(chat_manager.get_chat_history(chat_id)) == 0

if __name__ == "__main__":
    pytest.main([__file__]) 