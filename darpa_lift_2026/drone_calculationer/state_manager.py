"""
Global State Manager
Handles shared variables across all tabs
"""

from PyQt5.QtCore import QObject, pyqtSignal
import json


class GlobalState(QObject):
    """
    Centralized state management for the calculator.
    Allows tabs to share values and update reactively.
    """
    
    # Signal emitted when any value changes
    state_changed = pyqtSignal()
    
    def __init__(self):
        super().__init__()
        self._values = {}
        
    def set_value(self, key, value):
        """Set a value and emit change signal"""
        old_value = self._values.get(key)
        self._values[key] = value
        
        # Only emit if value actually changed
        if old_value != value:
            self.state_changed.emit()
            
    def get_value(self, key, default=None):
        """Get a value with optional default"""
        return self._values.get(key, default)
        
    def get_all(self):
        """Get all stored values"""
        return self._values.copy()
        
    def clear(self):
        """Clear all values"""
        self._values.clear()
        self.state_changed.emit()
        
    def to_dict(self):
        """Convert state to dictionary for serialization"""
        return self._values.copy()
        
    def from_dict(self, data_dict):
        """Load state from dictionary"""
        self._values.clear()
        self._values.update(data_dict)
        self.state_changed.emit()