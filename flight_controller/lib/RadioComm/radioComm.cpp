/*
 * Radio Communication Implementation
 * Complete single-file approach for all receiver protocols
 * Based on original dRehmFlight radioComm.ino with all features
 */

#include "radioComm.h"

//========================================================================================================================//
//                                            PROTOCOL-SPECIFIC INCLUDES                                                  //
//========================================================================================================================//

#ifdef USE_SBUS_RECEIVER
    #include "SBUS.h"
    SBUS sbus(SBUS_SERIAL_PORT);
    uint16_t sbusChannels[16];
    bool sbusFailSafe = false;
    bool sbusLostFrame = false;
#endif

#ifdef USE_DSM_RECEIVER
    #include "DSMRX.h"
    DSM1024 DSM;  // Match original naming
#endif

//========================================================================================================================//
//                                              CHANNEL VARIABLES                                                         //
//========================================================================================================================//

// Channel values in microseconds (1000-2000)
unsigned long channel_1_pwm = 1500;
unsigned long channel_2_pwm = 1500;
unsigned long channel_3_pwm = 1000;  // Throttle starts at minimum
unsigned long channel_4_pwm = 1500;
unsigned long channel_5_pwm = 1000;
unsigned long channel_6_pwm = 1000;

// For PPM/PWM - raw channel storage (used internally)
static unsigned long channel_1_raw = 1500;
static unsigned long channel_2_raw = 1500;
static unsigned long channel_3_raw = 1000;
static unsigned long channel_4_raw = 1500;
static unsigned long channel_5_raw = 1000;
static unsigned long channel_6_raw = 1000;

// Interrupt timing variables for PPM/PWM
static unsigned long rising_edge_start_1, rising_edge_start_2, rising_edge_start_3;
static unsigned long rising_edge_start_4, rising_edge_start_5, rising_edge_start_6;
static int ppm_counter = 0;
static unsigned long time_ms = 0;

// Last frame time for timeout detection
unsigned long lastFrameTime = 0;

//========================================================================================================================//
//                                        INTERRUPT SERVICE ROUTINES                                                      //
//========================================================================================================================//

// PPM interrupt handler
void getPPM() {
    #ifdef USE_PPM_RECEIVER
        unsigned long dt_ppm;
        int trig = digitalRead(PPM_PIN);
        
        if (trig == 1) { // Only care about rising edge
            dt_ppm = micros() - time_ms;
            time_ms = micros();
            
            if (dt_ppm > 5000) { // Waiting for long pulse to indicate new pulse train
                ppm_counter = 0;
            }
            
            if (ppm_counter == 1) { // First pulse
                channel_1_raw = dt_ppm;
            }
            if (ppm_counter == 2) { // Second pulse
                channel_2_raw = dt_ppm;
            }
            if (ppm_counter == 3) { // Third pulse
                channel_3_raw = dt_ppm;
            }
            if (ppm_counter == 4) { // Fourth pulse
                channel_4_raw = dt_ppm;
            }
            if (ppm_counter == 5) { // Fifth pulse
                channel_5_raw = dt_ppm;
            }
            if (ppm_counter == 6) { // Sixth pulse
                channel_6_raw = dt_ppm;
            }
            
            ppm_counter = ppm_counter + 1;
        }
    #endif
}

// PWM interrupt handlers
void getCh1() {
    #ifdef USE_PWM_RECEIVER
        int trigger = digitalRead(PWM_CH1_PIN);
        if (trigger == 1) {
            rising_edge_start_1 = micros();
        }
        else if (trigger == 0) {
            channel_1_raw = micros() - rising_edge_start_1;
        }
    #endif
}

void getCh2() {
    #ifdef USE_PWM_RECEIVER
        int trigger = digitalRead(PWM_CH2_PIN);
        if (trigger == 1) {
            rising_edge_start_2 = micros();
        }
        else if (trigger == 0) {
            channel_2_raw = micros() - rising_edge_start_2;
        }
    #endif
}

void getCh3() {
    #ifdef USE_PWM_RECEIVER
        int trigger = digitalRead(PWM_CH3_PIN);
        if (trigger == 1) {
            rising_edge_start_3 = micros();
        }
        else if (trigger == 0) {
            channel_3_raw = micros() - rising_edge_start_3;
        }
    #endif
}

void getCh4() {
    #ifdef USE_PWM_RECEIVER
        int trigger = digitalRead(PWM_CH4_PIN);
        if (trigger == 1) {
            rising_edge_start_4 = micros();
        }
        else if (trigger == 0) {
            channel_4_raw = micros() - rising_edge_start_4;
        }
    #endif
}

void getCh5() {
    #ifdef USE_PWM_RECEIVER
        int trigger = digitalRead(PWM_CH5_PIN);
        if (trigger == 1) {
            rising_edge_start_5 = micros();
        }
        else if (trigger == 0) {
            channel_5_raw = micros() - rising_edge_start_5;
        }
    #endif
}

void getCh6() {
    #ifdef USE_PWM_RECEIVER
        int trigger = digitalRead(PWM_CH6_PIN);
        if (trigger == 1) {
            rising_edge_start_6 = micros();
        }
        else if (trigger == 0) {
            channel_6_raw = micros() - rising_edge_start_6;
        }
    #endif
}

//========================================================================================================================//
//                                              RADIO SETUP                                                               //
//========================================================================================================================//

void radioSetup() {
    #ifdef USE_SBUS_RECEIVER
        sbus.begin();
        Serial.println("SBUS receiver initialized");
        Serial.print("  Port: Serial"); 
        #if (SBUS_SERIAL_PORT == Serial5)
            Serial.println("5 (RX pin 21)");
        #elif (SBUS_SERIAL_PORT == Serial3)
            Serial.println("3 (RX pin 15)");
        #endif
        Serial.println("  Baud: 100000, 8E2 (inverted)");
        Serial.println("  Channels: 16");
        
    #elif defined(USE_DSM_RECEIVER)
        DSM_SERIAL_PORT.begin(115000);  // Match original 115000 baud
        Serial.println("DSM receiver initialized");
        Serial.println("  Port: Serial3 (RX pin 15)");
        Serial.println("  Baud: 115000");
        Serial.println("  Channels: 12");
        
    #elif defined(USE_PPM_RECEIVER)
        // PPM setup
        pinMode(PPM_PIN, INPUT_PULLUP);
        delay(20);
        attachInterrupt(digitalPinToInterrupt(PPM_PIN), getPPM, CHANGE);
        Serial.println("PPM receiver initialized");
        Serial.print("  Pin: "); Serial.println(PPM_PIN);
        
    #elif defined(USE_PWM_RECEIVER)
        // PWM setup - attach interrupts for each channel
        pinMode(PWM_CH1_PIN, INPUT_PULLUP);
        pinMode(PWM_CH2_PIN, INPUT_PULLUP);
        pinMode(PWM_CH3_PIN, INPUT_PULLUP);
        pinMode(PWM_CH4_PIN, INPUT_PULLUP);
        pinMode(PWM_CH5_PIN, INPUT_PULLUP);
        pinMode(PWM_CH6_PIN, INPUT_PULLUP);
        delay(20);
        attachInterrupt(digitalPinToInterrupt(PWM_CH1_PIN), getCh1, CHANGE);
        attachInterrupt(digitalPinToInterrupt(PWM_CH2_PIN), getCh2, CHANGE);
        attachInterrupt(digitalPinToInterrupt(PWM_CH3_PIN), getCh3, CHANGE);
        attachInterrupt(digitalPinToInterrupt(PWM_CH4_PIN), getCh4, CHANGE);
        attachInterrupt(digitalPinToInterrupt(PWM_CH5_PIN), getCh5, CHANGE);
        attachInterrupt(digitalPinToInterrupt(PWM_CH6_PIN), getCh6, CHANGE);
        delay(20);
        Serial.println("PWM receiver initialized");
        Serial.println("  Individual channel pins");
        
    #else
        #error "No receiver type defined in config.h!"
    #endif
    
    delay(100);
    lastFrameTime = millis();
}

//========================================================================================================================//
//                                              GET COMMANDS                                                              //
//========================================================================================================================//

void getCommands() {
    
    //========== SBUS RECEIVER ==========//
    #ifdef USE_SBUS_RECEIVER
        if (sbus.read(&sbusChannels[0], &sbusFailSafe, &sbusLostFrame)) {
            // SBUS channels are 0-2047, convert to 1000-2000 microseconds
            // Standard SBUS calibration: 172 = 1000us, 1811 = 2000us
            channel_1_pwm = map(sbusChannels[0], 172, 1811, 1000, 2000);  // Roll
            channel_2_pwm = map(sbusChannels[1], 172, 1811, 1000, 2000);  // Pitch
            channel_3_pwm = map(sbusChannels[2], 172, 1811, 1000, 2000);  // Throttle
            channel_4_pwm = map(sbusChannels[3], 172, 1811, 1000, 2000);  // Yaw
            channel_5_pwm = map(sbusChannels[4], 172, 1811, 1000, 2000);  // Aux1
            channel_6_pwm = map(sbusChannels[5], 172, 1811, 1000, 2000);  // Aux2
            
            lastFrameTime = millis();
        }
    
    //========== DSM RECEIVER ==========//
    #elif defined(USE_DSM_RECEIVER)
        if (!DSM.timedOut()) {
            // DSM already returns microseconds (1000-2000)
            channel_1_pwm = DSM.getChannelValue(1);  // Roll
            channel_2_pwm = DSM.getChannelValue(2);  // Pitch
            channel_3_pwm = DSM.getChannelValue(3);  // Throttle
            channel_4_pwm = DSM.getChannelValue(4);  // Yaw
            channel_5_pwm = DSM.getChannelValue(5);  // Aux1
            channel_6_pwm = DSM.getChannelValue(6);  // Aux2
            
            lastFrameTime = millis();
        }
    
    //========== PPM/PWM RECEIVER ==========//
    #elif defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
        // For PPM/PWM, interrupts update channel_X_raw variables
        // Copy to channel_X_pwm variables
        channel_1_pwm = channel_1_raw;
        channel_2_pwm = channel_2_raw;
        channel_3_pwm = channel_3_raw;
        channel_4_pwm = channel_4_raw;
        channel_5_pwm = channel_5_raw;
        channel_6_pwm = channel_6_raw;
        
        // Update last frame time if channels are changing
        if (channel_1_raw != 1500 || channel_2_raw != 1500) {
            lastFrameTime = millis();
        }
        
    #endif
    
    // Constrain all channels to valid range
    channel_1_pwm = constrain(channel_1_pwm, 1000, 2000);
    channel_2_pwm = constrain(channel_2_pwm, 1000, 2000);
    channel_3_pwm = constrain(channel_3_pwm, 1000, 2000);
    channel_4_pwm = constrain(channel_4_pwm, 1000, 2000);
    channel_5_pwm = constrain(channel_5_pwm, 1000, 2000);
    channel_6_pwm = constrain(channel_6_pwm, 1000, 2000);
}

//========================================================================================================================//
//                                         GET RADIO PWM (Original function)                                              //
//========================================================================================================================//

unsigned long getRadioPWM(int ch_num) {
    // Original dRehmFlight function to get channel value by number
    unsigned long returnPWM = 1500;  // Default center/safe value
    
    if (ch_num == 1) {
        returnPWM = channel_1_pwm;
    }
    else if (ch_num == 2) {
        returnPWM = channel_2_pwm;
    }
    else if (ch_num == 3) {
        returnPWM = channel_3_pwm;
    }
    else if (ch_num == 4) {
        returnPWM = channel_4_pwm;
    }
    else if (ch_num == 5) {
        returnPWM = channel_5_pwm;
    }
    else if (ch_num == 6) {
        returnPWM = channel_6_pwm;
    }
    
    return returnPWM;
}

//========================================================================================================================//
//                                              FAILSAFE                                                                  //
//========================================================================================================================//

void failSafe() {
    // Check for signal loss based on receiver type
    bool signalLost = false;
    
    #ifdef USE_SBUS_RECEIVER
        // SBUS has built-in failsafe flags
        signalLost = sbusFailSafe || sbusLostFrame;
        
        // Also check for timeout (no updates in 500ms)
        if (millis() - lastFrameTime > 500) {
            signalLost = true;
        }
        
    #elif defined(USE_DSM_RECEIVER)
        // DSM has built-in timeout detection
        signalLost = DSM.timedOut();
        
    #elif defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
        // Check for timeout (no updates in 500ms)
        if (millis() - lastFrameTime > 500) {
            signalLost = true;
        }
    #endif
    
    // If signal is lost, use failsafe values
    if (signalLost) {
        channel_1_pwm = FAILSAFE_ROLL;      // Center stick
        channel_2_pwm = FAILSAFE_PITCH;     // Center stick
        channel_3_pwm = FAILSAFE_THROTTLE;  // Minimum throttle
        channel_4_pwm = FAILSAFE_YAW;       // Center stick
        channel_5_pwm = FAILSAFE_AUX1;      // Throttle cut (high)
        channel_6_pwm = FAILSAFE_AUX2;      // Default value
        
        // Blink LED rapidly to indicate failsafe
        if (millis() % 200 < 100) {
            digitalWrite(LED_PIN, HIGH);
        } else {
            digitalWrite(LED_PIN, LOW);
        }
    }
}

//========================================================================================================================//
//                                        CHECK RECEIVER CONNECTION                                                       //
//========================================================================================================================//

bool isReceiverConnected() {
    #ifdef USE_SBUS_RECEIVER
        return !sbusFailSafe && !sbusLostFrame && (millis() - lastFrameTime < 500);
        
    #elif defined(USE_DSM_RECEIVER)
        return !DSM.timedOut();
        
    #elif defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
        return (millis() - lastFrameTime < 500);
        
    #else
        return false;
    #endif
}

//========================================================================================================================//
//                                         DSM SERIAL EVENT HANDLER                                                       //
//========================================================================================================================//

// For DSM receivers - this is called automatically when serial data arrives
void serialEvent3() {
    #ifdef USE_DSM_RECEIVER
        while (DSM_SERIAL_PORT.available()) {
            DSM.handleSerialEvent(DSM_SERIAL_PORT.read(), micros());
        }
    #endif
}