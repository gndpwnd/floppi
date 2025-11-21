"""
VTOL Drone Design Calculator
Main application file - Run this to start the calculator
PyQt5 Version
"""

import sys
import json
import os
from PyQt5.QtWidgets import (QApplication, QMainWindow, QTabWidget, QVBoxLayout, 
                             QWidget, QMenuBar, QMenu, QAction, QMessageBox, 
                             QInputDialog, QFileDialog, QToolBar, QPushButton,
                             QHBoxLayout, QLabel, QTextEdit)
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QIcon

# Import state manager and all tabs
from state_manager import GlobalState
from tabs import (
    ConstantsSafetyTab,
    PropellerTab,
    MotorsTab,
    BatteryTab,
    AirframeTab,
    FixedWingTab,
    MissionProfileTab,
    WeatherTab,
    HoverPerformanceTab,
    CruisePerformanceTab,
    TurnAnalysisTab,
    EnergyBudgetTab,
    PerformanceSummaryTab
)


class UsageTab(QWidget):
    """Tab showing usage instructions and keyboard shortcuts"""
    
    def __init__(self):
        super().__init__()
        self.init_ui()
        
    def init_ui(self):
        layout = QVBoxLayout()
        
        # Title
        title_label = QLabel("<h1>VTOL Calculator Usage Guide</h1>")
        layout.addWidget(title_label)
        
        # Instructions
        instructions = QTextEdit()
        instructions.setReadOnly(True)
        instructions.setHtml("""
        <h2>Quick Start</h2>
        <ol>
        <li>Start with the <b>Constants & Safety</b> tab to set your safety factors and constants</li>
        <li>Configure your components in order: Propeller → Motors → Battery → Airframe → Fixed Wing</li>
        <li>Set your mission parameters in the <b>Mission Profile</b> tab</li>
        <li>Check environmental conditions in the <b>Weather</b> tab</li>
        <li>Review performance calculations in the remaining tabs</li>
        <li>Check the <b>Performance Summary</b> for overall status</li>
        </ol>
        
        <h2>Keyboard Shortcuts</h2>
        <table border="1" style="border-collapse: collapse; width: 100%;">
        <tr><th style="padding: 8px;">Action</th><th style="padding: 8px;">Shortcut</th></tr>
        <tr><td style="padding: 8px;">Save Configuration</td><td style="padding: 8px;">Ctrl+S</td></tr>
        <tr><td style="padding: 8px;">Load Configuration</td><td style="padding: 8px;">Ctrl+L</td></tr>
        <tr><td style="padding: 8px;">Reset to Defaults</td><td style="padding: 8px;">Ctrl+R</td></tr>
        <tr><td style="padding: 8px;">Next Tab</td><td style="padding: 8px;">Ctrl+Tab</td></tr>
        <tr><td style="padding: 8px;">Previous Tab</td><td style="padding: 8px;">Ctrl+Shift+Tab</td></tr>
        </table>
        
        <h2>Configuration Management</h2>
        <p><b>Save Configuration:</b> Save all current settings to a JSON file. You can name your configuration and choose where to save it.</p>
        <p><b>Load Configuration:</b> Load previously saved settings from a JSON file.</p>
        <p><b>Reset to Defaults:</b> Clear all inputs and restore default values.</p>
        
        <h2>Tips</h2>
        <ul>
        <li>Changes in one tab automatically update dependent calculations in other tabs</li>
        <li>Use the toolbar buttons for quick access to save/load operations</li>
        <li>Save different configurations for different design approaches</li>
        <li>Check the Performance Summary tab for overall design status</li>
        <li>All equations are displayed in each tab for reference</li>
        </ul>
        """)
        layout.addWidget(instructions)
        
        self.setLayout(layout)


class VTOLCalculator(QMainWindow):
    """Main application window"""
    
    def __init__(self):
        super().__init__()
        self.setWindowTitle("VTOL Drone Design Calculator - DARPA Lift Challenge")
        self.setGeometry(100, 100, 1400, 900)
        
        # Initialize global state
        self.state = GlobalState()
        
        # Create main widget and layout
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        layout = QVBoxLayout(main_widget)
        
        # Create menu bar and toolbar
        self.create_menu_bar()
        self.create_toolbar()
        
        # Create tab widget
        self.tabs = QTabWidget()
        layout.addWidget(self.tabs)
        
        # Add all tabs
        self.add_tabs()
        
        # Apply styling
        self.apply_styling()
        
    def create_menu_bar(self):
        """Create the menu bar with file operations"""
        menubar = self.menuBar()
        
        # File menu
        file_menu = menubar.addMenu('File')
        
        # Save configuration action
        self.save_action = QAction('Save Configuration', self)
        self.save_action.setShortcut('Ctrl+S')
        self.save_action.triggered.connect(self.save_configuration)
        file_menu.addAction(self.save_action)
        
        # Load configuration action
        self.load_action = QAction('Load Configuration', self)
        self.load_action.setShortcut('Ctrl+L')
        self.load_action.triggered.connect(self.load_configuration)
        file_menu.addAction(self.load_action)
        
        # Reset configuration action
        self.reset_action = QAction('Reset to Defaults', self)
        self.reset_action.setShortcut('Ctrl+R')
        self.reset_action.triggered.connect(self.reset_configuration)
        file_menu.addAction(self.reset_action)
        
    def create_toolbar(self):
        """Create toolbar with save/load buttons"""
        toolbar = QToolBar("Main Toolbar")
        self.addToolBar(toolbar)
        
        # Save button
        save_btn = QPushButton("Save Configuration")
        save_btn.clicked.connect(self.save_configuration)
        save_btn.setToolTip("Save current configuration (Ctrl+S)")
        toolbar.addWidget(save_btn)
        
        # Load button
        load_btn = QPushButton("Load Configuration")
        load_btn.clicked.connect(self.load_configuration)
        load_btn.setToolTip("Load saved configuration (Ctrl+L)")
        toolbar.addWidget(load_btn)
        
        # Reset button
        reset_btn = QPushButton("Reset to Defaults")
        reset_btn.clicked.connect(self.reset_configuration)
        reset_btn.setToolTip("Reset all inputs to defaults (Ctrl+R)")
        toolbar.addWidget(reset_btn)
        
        # Spacer
        toolbar.addWidget(QLabel("   "))  # Simple spacer
        
        # Quick access label
        toolbar.addWidget(QLabel("Quick Access: "))
        
    def add_tabs(self):
        """Add all calculation tabs"""
        self.tab_instances = [
            ("Usage Guide", UsageTab()),
            ("Constants & Safety", ConstantsSafetyTab(self.state)),
            ("Propeller", PropellerTab(self.state)),
            ("Motors", MotorsTab(self.state)),
            ("Battery", BatteryTab(self.state)),
            ("Airframe", AirframeTab(self.state)),
            ("Fixed Wing", FixedWingTab(self.state)),
            ("Mission Profile", MissionProfileTab(self.state)),
            ("Weather", WeatherTab(self.state)),
            ("Hover Performance", HoverPerformanceTab(self.state)),
            ("Cruise Performance", CruisePerformanceTab(self.state)),
            ("Turn Analysis", TurnAnalysisTab(self.state)),
            ("Energy Budget", EnergyBudgetTab(self.state)),
            ("Performance Summary", PerformanceSummaryTab(self.state))
        ]
        
        for name, tab in self.tab_instances:
            self.tabs.addTab(tab, name)
            
    def apply_styling(self):
        """Apply application-wide styling"""
        self.setStyleSheet("""
            QMainWindow {
                background-color: #f0f0f0;
            }
            QTabWidget::pane {
                border: 1px solid #cccccc;
                background-color: white;
            }
            QTabBar::tab {
                background-color: #e0e0e0;
                padding: 8px 16px;
                margin-right: 2px;
                border: 1px solid #cccccc;
            }
            QTabBar::tab:selected {
                background-color: white;
                font-weight: bold;
                border-bottom: 2px solid #4CAF50;
            }
            QTabBar::tab:hover {
                background-color: #d0d0d0;
            }
            QGroupBox {
                font-weight: bold;
                border: 2px solid #cccccc;
                border-radius: 5px;
                margin-top: 10px;
                padding-top: 10px;
                background-color: #fafafa;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
                background-color: #fafafa;
            }
            QLineEdit {
                padding: 5px;
                border: 1px solid #cccccc;
                border-radius: 3px;
                background-color: white;
            }
            QLineEdit:focus {
                border: 2px solid #4CAF50;
            }
            QLabel {
                padding: 2px;
            }
            QPushButton {
                background-color: #4CAF50;
                border: none;
                color: white;
                padding: 8px 16px;
                border-radius: 4px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #45a049;
            }
            QPushButton:pressed {
                background-color: #3d8b40;
            }
            QToolBar {
                background-color: #e0e0e0;
                border: none;
                spacing: 5px;
                padding: 5px;
            }
            QTextEdit {
                background-color: white;
                border: 1px solid #cccccc;
                border-radius: 4px;
                padding: 10px;
            }
        """)
            
    def save_configuration(self):
        """Save current configuration to JSON file"""
        # Get configuration name from user
        config_name, ok = QInputDialog.getText(
            self, 
            'Save Configuration',
            'Enter configuration name:',
            text='VTOL_Configuration'
        )
        
        if not ok or not config_name:
            return
            
        # Get save location from user
        file_path, selected_filter = QFileDialog.getSaveFileName(
            self,
            'Save Configuration',
            f'{config_name}.json',
            'JSON Files (*.json);;All Files (*)'
        )
        
        if not file_path:
            return
            
        # Ensure .json extension
        if not file_path.lower().endswith('.json'):
            file_path += '.json'
            
        try:
            # Collect all input values from tabs
            configuration_data = {
                'metadata': {
                    'name': config_name,
                    'version': '1.0',
                    'description': 'VTOL Drone Configuration',
                    'timestamp': self.get_timestamp()
                },
                'state_data': self.state.to_dict(),
                'tab_data': self.collect_tab_data()
            }
            
            # Save to file
            with open(file_path, 'w') as f:
                json.dump(configuration_data, f, indent=2)
                
            QMessageBox.information(
                self, 
                'Success', 
                f'Configuration "{config_name}" saved successfully!\n\nLocation: {file_path}'
            )
            
        except Exception as e:
            QMessageBox.critical(
                self,
                'Save Error',
                f'Failed to save configuration: {str(e)}'
            )
            
    def load_configuration(self):
        """Load configuration from JSON file"""
        # Get file location from user
        file_path, selected_filter = QFileDialog.getOpenFileName(
            self,
            'Load Configuration',
            '',
            'JSON Files (*.json);;All Files (*)'
        )
        
        if not file_path:
            return
            
        try:
            # Load from file
            with open(file_path, 'r') as f:
                configuration_data = json.load(f)
                
            # Restore state data
            if 'state_data' in configuration_data:
                self.state.from_dict(configuration_data['state_data'])
                
            # Restore tab data
            if 'tab_data' in configuration_data:
                self.restore_tab_data(configuration_data['tab_data'])
                
            config_name = configuration_data.get('metadata', {}).get('name', 'Unknown')
            
            QMessageBox.information(
                self,
                'Success',
                f'Configuration "{config_name}" loaded successfully!'
            )
            
        except Exception as e:
            QMessageBox.critical(
                self,
                'Load Error',
                f'Failed to load configuration: {str(e)}'
            )
            
    def reset_configuration(self):
        """Reset all inputs to default values"""
        reply = QMessageBox.question(
            self,
            'Reset Configuration',
            'Are you sure you want to reset all inputs to default values?',
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No
        )
        
        if reply == QMessageBox.Yes:
            self.state.clear()
            # Recreate tabs to reset inputs
            current_tab_index = self.tabs.currentIndex()
            self.tabs.clear()
            self.add_tabs()
            self.tabs.setCurrentIndex(current_tab_index)
            QMessageBox.information(
                self,
                'Reset Complete',
                'All inputs have been reset to default values.'
            )
            
    def collect_tab_data(self):
        """Collect input values from all tabs"""
        tab_data = {}
        
        for tab_name, tab_instance in self.tab_instances:
            # Skip UsageTab as it has no inputs
            if hasattr(tab_instance, 'get_input_values'):
                tab_inputs = tab_instance.get_input_values()
                tab_data[tab_name] = tab_inputs
            
        return tab_data
        
    def restore_tab_data(self, tab_data):
        """Restore input values to all tabs"""
        for tab_name, tab_inputs in tab_data.items():
            # Find the corresponding tab instance
            for instance_name, tab_instance in self.tab_instances:
                if instance_name == tab_name and hasattr(tab_instance, 'set_input_values'):
                    tab_instance.set_input_values(tab_inputs)
                    break
                    
    def get_timestamp(self):
        """Get current timestamp for metadata"""
        from datetime import datetime
        return datetime.now().isoformat()
        
    def keyPressEvent(self, event):
        """Handle keyboard shortcuts"""
        if event.modifiers() == Qt.ControlModifier:
            if event.key() == Qt.Key_S:
                self.save_configuration()
            elif event.key() == Qt.Key_L:
                self.load_configuration()
            elif event.key() == Qt.Key_R:
                self.reset_configuration()
        super().keyPressEvent(event)


def main():
    """Application entry point"""
    app = QApplication(sys.argv)
    app.setStyle('Fusion')  # Modern look
    
    calculator = VTOLCalculator()
    calculator.show()
    
    sys.exit(app.exec_())


if __name__ == '__main__':
    main()