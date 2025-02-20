#!/usr/bin/env python3
"""
GQRX Tuning Tool for LangChain
Provides frequency tuning capability for AI agents
"""

from langchain.tools import BaseTool
from typing import Optional, Type, Literal
from pydantic import BaseModel, Field
import socket
import time

class TuneFrequencyInput(BaseModel):
    """Input for the tune frequency tool"""
    frequency: int = Field(..., description="Frequency in Hz to tune to")

class GqrxTuningTool(BaseTool):
    name: Literal["tune_frequency"] = "tune_frequency"
    description: str = """
    Tunes the radio receiver to a specified frequency.
    Input should be a frequency in Hz (e.g., 145000000 for 145 MHz).
    The tool will attempt to tune to the specified frequency and return success/failure status.
    """
    args_schema: Type[BaseModel] = TuneFrequencyInput
    
    # Connection settings
    host: str = Field(default="127.0.0.1")
    port: int = Field(default=7356)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._socket = None

    def _connect(self) -> bool:
        """Connect to GQRX's remote control interface"""
        try:
            self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._socket.connect((self.host, self.port))
            return True
        except Exception as e:
            return False

    def _disconnect(self):
        """Close the connection"""
        if self._socket:
            self._socket.close()
            self._socket = None

    def _send_command(self, command: str) -> Optional[str]:
        """Send a command to GQRX and get the response"""
        try:
            self._socket.send((command + '\n').encode())
            return self._socket.recv(1024).decode().strip()
        except Exception:
            return None

    def _run(self, frequency: int) -> str:
        """Run the tool with the specified frequency"""
        # Connect to GQRX
        if not self._connect():
            return "Failed to connect to GQRX. Is it running with remote control enabled?"

        try:
            # Set frequency
            response = self._send_command(f"F {frequency}")
            if response != "RPRT 0":
                return f"Failed to set frequency: {response}"

            # Verify frequency
            time.sleep(0.5)  # Give GQRX time to tune
            current_freq = self._send_command("f")
            
            try:
                current_freq = int(current_freq)
                if abs(current_freq - frequency) < 1:  # Allow for minor rounding
                    return f"Successfully tuned to {frequency} Hz"
                else:
                    return f"Frequency mismatch: Requested {frequency} Hz, Got {current_freq} Hz"
            except:
                return f"Failed to verify frequency: {current_freq}"

        finally:
            self._disconnect()

    async def _arun(self, frequency: int) -> str:
        """Async implementation of the tool"""
        # For now, just call the sync version
        return self._run(frequency)

# Example usage:
if __name__ == "__main__":
    # Create tool instance
    tool = GqrxTuningTool()
    
    # Test with a frequency
    result = tool.run({"frequency": 145000000})  # Tune to 145 MHz
    print(result) 