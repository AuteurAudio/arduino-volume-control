/* 
 * Arduino Based Volume Control System 
 * Copyright (c) 2013 - 2015, Colin Shaw
 * Distributed under the terms of the MIT License
 */
 
 
 
 /*
  * General pin definitions
  */
 
 
// Relating to volume control via encoder 
#define volumeKnobPin1         3         // INPUT Encoder pin (works best with interrupt pins)
#define volumeKnobPin2         2         // INPUT Encoder pin (switch pins to reverse direction)

// Relating to the PGA4311
#define volumeClockPin         8         // OUTPUT Clock pin for volume control
#define volumeDataPin          11         // OUTPUT Data pin for volume control
#define volumeSelectPin        10        // OUTPUT Select pin for volume control
#define volumeMutePin          9        // OUTPUT Mute pin for the volume control  

// Relating to debouncing the mute and channel select pins
#define muteSwitchPin          5         // INPUT Momentary switch for mute
#define channelSwitchPin       6         // Toggle input for channel select
#define outputSwitchPin        4         // Toggle for impedance control for output
#define switchDebounceTime     50        // Milliseconds switch needs to be in state before effect (ms)

// Output pin to control input channel relay and output impedance shunt relay
#define channelRelayPin        12        // Pin to control the relay for channel selection
#define outputRelayPin         7         // Pin to control relay for output for turnout delay

// Input pin for selecting balance selection
#define balanceSwitchPin       13        // This default is the LED output pin on a stock
                                         // Arduino -- make sure you are dragging the pin 
                                         // with enough oomph to get past the LED circuit


// XOR logic reversal for switches and relays
// Somewhat redundant, but allows the logic states to be deterministic on both input and output
#define channelSwitchReverse   HIGH
#define channelRelayReverse    HIGH
#define outputSwitchReverse    HIGH
#define outputRelayReverse     HIGH
#define balanceSwitchReverse   LOW



// Maximum (byte value) of the volume to send to the PGA2311
// This is here to avoid regions where the high gains has too high S/N
// (192 is 0 dB -- e.g. no gain)
#define maximumVolume          192                


// Default definition for balance 
// If this is set then it overrides the setting of balance 
// This is a number added to the right channel in 0.5dB increments
// (e.g. -2 means right is 1 dB quieter than left) 
// Comment out if you want to use the feauture, 
// uncomment to use the fixed value
#define balanceValue          -1



/*
 * EEPROM usage map (use imagination if XOR logic is changed
 *
 * 0: Balance setpoint if compiled 
 * 3: Volume for channel one / output one
 * 4: Volume for channel one / output two
 * 5: Volume for channel two / output one
 * 6: Volume for channel two / output two
 */
 


/*
 * Includes, variables and some options
 */
 
 
//#define ENCODER_DO_NOT_USE_INTERRUPTS          // An option for the Encoder library
#include <Encoder.h>
#include <Bounce.h>
#include <EEPROM.h>

 
// Encoder instantiation  
Encoder   volumeKnob(volumeKnobPin1, volumeKnobPin2);
long      newVolumePosition;
long      tempChannelVolume;
byte      channelVolume;
long int  delayTimeout;

// Debouncer instantiation
Bounce    muteSwitch =       Bounce(muteSwitchPin,switchDebounceTime);
Bounce    channelSwitch =    Bounce(channelSwitchPin,switchDebounceTime);
Bounce    outputSwitch =     Bounce(outputSwitchPin,switchDebounceTime);
Bounce    balanceSwitch =    Bounce(balanceSwitchPin,switchDebounceTime);

// Relating to balance
int       balanceConstant =  0;                // Default to zero until it is loaded

// Other variables
int       isMuted =          false;            // Internally used status for mute state
int       channelRun =       false;            // Internally used status for passing control to channel 
int       outputRun =        false;            // Internally used status for passing control to output 
long int  selectedChannel;
long int  selectedOutput;



/*
 * Inline function to write a byte to the PGA2311
 */

static inline void byteWrite(byte byteOut){
   for (byte i=0;i<8;i++) {
     digitalWrite(volumeClockPin, LOW);
     if (0x80 & byteOut) digitalWrite(volumeDataPin, HIGH);
     else digitalWrite(volumeDataPin, LOW);
     digitalWrite(volumeClockPin, HIGH);
     digitalWrite(volumeClockPin, LOW);
     byteOut<<=1;   
   }
}
 
 
 
/*
 * Function to set the (stereo) volume on the PGA2311
 */

void setVolume(long volume){
   
   long int r_vol_test;
   
   byte l_vol=(byte)volume;
   byte r_vol=0;
   
   l_vol=volume;
   r_vol_test=volume+balanceConstant;
  
   // This implies that balanceConstant>0 
   // This test is unlikely to run unless maximumVolume is 255 or very close
   if(r_vol_test>255){
      r_vol=255;
      l_vol=255-balanceConstant;
   }
   // This implies that balanceConstant<0... both channels should be off at once
   else if(r_vol_test<0){
     r_vol=0;
     l_vol=0;
   }
   // Business as usual  
   else{
     r_vol=(byte)r_vol_test;
   }
       
   digitalWrite(volumeSelectPin, LOW);   
   byteWrite(r_vol);                                // Right        
   byteWrite(l_vol);                                // Left
   digitalWrite(volumeSelectPin, HIGH);    
   digitalWrite(volumeClockPin, HIGH);
   digitalWrite(volumeDataPin, HIGH);
}



/*
 * Function to scale volume from one level to another (softer changes for mute)
 */

void scaleVolume(byte startVolume, byte endVolume, byte volumeSteps){
  byte diff;
  long counter;
  
  if(endVolume==startVolume){
    return;
  }
  if(endVolume>startVolume){
    Serial.print("Increasing volume to (byte value) ");
    Serial.println(endVolume);
    diff=(endVolume-startVolume)/volumeSteps;
    
    // Protect against a non-event
    if(diff==0){
      diff=1;
    }
    counter=startVolume;
    while(counter<endVolume){
      setVolume(counter);
      delay(25);  
      counter+=diff;  
    }
    setVolume(endVolume);               
  }  
  else{
    Serial.print("Diminishing volume to (byte value) ");
    Serial.println(endVolume);
    diff=(startVolume-endVolume)/volumeSteps;
    
    // Protect against a non-event
    if(diff==0){
      diff=1;
    }
    counter=startVolume;
    while(counter>endVolume){
      setVolume(counter);
      delay(25);  
      counter-=diff; 
    }
    setVolume(endVolume);              
  }  
  return;
}  




/*
 * Main Arduino setup call
 */


void setup(){
  
  // Initialize USB serial feedback for printing
  Serial.begin(38400);
  Serial.println("Initializing...");
  
  // Mute the PGA2311
  pinMode(volumeMutePin,OUTPUT);
  digitalWrite(volumeMutePin,LOW);           
  
  // Set up control pins for PGA2311
  pinMode(volumeSelectPin,OUTPUT);
  pinMode(volumeClockPin,OUTPUT);
  pinMode(volumeDataPin,OUTPUT);
  digitalWrite(volumeSelectPin,HIGH);
  digitalWrite(volumeClockPin,HIGH);
  digitalWrite(volumeDataPin,HIGH);
  
  // Initialize other I/O pins
  pinMode(muteSwitchPin,INPUT);
  pinMode(channelSwitchPin,INPUT);
  pinMode(outputSwitchPin,INPUT);
  pinMode(channelRelayPin,OUTPUT);
  pinMode(outputRelayPin,OUTPUT);
  #ifndef balanceValue
    Serial.println("Setting balance input pin...");
    pinMode(balanceSwitchPin,INPUT);
  #endif
  
  // Delay a bit to wait for the state of the switches
  delay(switchDebounceTime+50);
 
  // Load the existing balance setting if we are using dynamic setting
  #ifndef balanceValue
    Serial.print("Loading existing balance = ");
    // Adjust by have a byte for the signed issue
    balanceConstant=EEPROM.read(0)-127;
    Serial.println(balanceConstant);
  #endif
  
  // Load the balance constant if it is static
  #ifdef balanceValue
    Serial.println("Using static balance...");
    balanceConstant=balanceValue;
  #endif
  
  
  // Read the channel switch and set the appropriate volume
  channelSwitch.update();
  if(channelSwitch.read()==(HIGH^channelSwitchReverse)){
    // Channel One   
    selectedChannel=1;
    Serial.println("Using channel one...");
    digitalWrite(channelRelayPin,HIGH^channelRelayReverse);
  }
  else{
    // Channel Two
    selectedChannel=2;
    Serial.println("Using channel two...");
    digitalWrite(channelRelayPin,LOW^channelRelayReverse);
  }  
  
  // Set volume to 0 for later soft volume start
  setVolume(0);
  
  // Read the output relay switch and set it appropriately
  outputSwitch.update();
  if(outputSwitch.read()==(HIGH^outputSwitchReverse)){
    selectedOutput=1;
    Serial.println("Using output channel one...");
    digitalWrite(outputRelayPin,HIGH^outputRelayReverse);
    channelVolume=EEPROM.read(2*selectedOutput+selectedChannel);
  }
  else{
    selectedOutput=2;
    Serial.println("Using output channel two...");
    digitalWrite(outputRelayPin,LOW^outputRelayReverse);
    channelVolume=EEPROM.read(2*selectedOutput+selectedChannel);
    
    // Safegaurd for the maximum
    if(channelVolume>maximumVolume){
      channelVolume=maximumVolume;
    }
    
  }  
  
  tempChannelVolume=channelVolume;
  
  Serial.print("Setting initial volume to (byte value) ");
  Serial.println(channelVolume);
  
  // Wait a bit for the whole system to come online
  delay(800);
  
  // Unmute the PGA2311
  digitalWrite(volumeMutePin,HIGH); 

  // Wait a bit
  delay(200);  
  
  // Smoothly scale into last volume
  scaleVolume(0,channelVolume,50);
  
  // Reset the encoder monitor  
  volumeKnob.write(0);
  
}

 
 
 
/*
 * Main Arduino loop 
 */
 
void loop(){
  
  
    /*
     * BALANCE
     */
     
     #ifndef balanceValue
     
         // Just update it, don't test on it
         balanceSwitch.update();
           
         // If it is a go then get into the below blocking loop  
         if(balanceSwitch.read()==(HIGH^balanceSwitchReverse)){  
        
           Serial.println("Entering balance adjust mode...");
            
           // Stick to this loop while the balance pin is engaged
           while(balanceSwitch.read()==(HIGH^balanceSwitchReverse)){
             
             // Get the new reading from the encoder 
             // (Yes, the names are funny since the default behavior and variables are for volume)
             newVolumePosition=volumeKnob.read();
             
             if (newVolumePosition != 0) {
             
               // Wait for the faux debounce  
               if(millis()-delayTimeout>5){
                 
                   // Adjust the balance
                   Serial.print("Changing balance setting to ");
                   balanceConstant+=newVolumePosition;
                   Serial.println(balanceConstant);
                   
                   // Save the new value, making it positive and centered in the byte
                   EEPROM.write(0,balanceConstant+127);
                   
                   // Set the volume so that the change takes effect
                   setVolume(channelVolume);
                   
                   // Reset the knob position
                   volumeKnob.write(0);
                   delayTimeout=millis();  
                
                }
                else{
                  volumeKnob.write(0);
                } 
            } 
            balanceSwitch.update();
         } 
       }
     
     #endif
     
  
    /*
     * OUTPUT SWITCH
     */
     
     // Only need to do this on switch state change
     if(outputSwitch.update() || outputRun){
       
       // Address the change
       if(outputSwitch.read()==(HIGH^outputSwitchReverse)){
         
         // Channel one
         if(selectedOutput==2){
            Serial.println("Switching to output one...");
            selectedOutput=1;
       
            // Scale the volume down if it is not muted
            if(isMuted==false && outputRun==false){
              scaleVolume(channelVolume,0,35);
       
              // Mute the PGA2311
              digitalWrite(volumeMutePin,LOW); 
            }
            // Reset the control variable
            outputRun=false;
            
            // Load the volume value
            channelVolume=EEPROM.read(2*selectedOutput+selectedChannel);
            
            // Safegaurd for the maximum
            if(channelVolume>maximumVolume){
              channelVolume=maximumVolume;
            }
            tempChannelVolume=channelVolume;
            
            // Switch the channel relay
            digitalWrite(outputRelayPin,HIGH^outputRelayReverse);
            
            // Delay a bit for the relay to switch
            delay(200);
            
            if(isMuted==false){
              // Unmute the PGA2311
              digitalWrite(volumeMutePin,HIGH); 
              
              // Delay 
              delay(20);
              
              // Check the status of the channel switch 
              if(channelSwitch.update()){
                // Just set the flag to run through the channel logic
                channelRun=true;
              }
              else{
                // Otherwise scale the volume back up
                scaleVolume(0,channelVolume,50);
              }
            }
         }
       }

       // Channel two
       else{
         if(selectedOutput==1){
            Serial.println("Switching to output two...");
            selectedOutput=2;
        
            // Scale the volume down if it is not muted
            if(isMuted==false && outputRun==false){
              scaleVolume(channelVolume,0,35);
              
              // Mute the PGA2311
              digitalWrite(volumeMutePin,LOW); 
            }  
            // Reset the control variable
            outputRun=false;
              
            // Load the volume value
            channelVolume=EEPROM.read(2*selectedOutput+selectedChannel);
            
            // Safegaurd for the maximum
            if(channelVolume>maximumVolume){
              channelVolume=maximumVolume;
            }
            tempChannelVolume=channelVolume;
            
            // Switch the channel relay
            digitalWrite(outputRelayPin,LOW^outputRelayReverse);
            
            // delay a bit for the relay to switch
            delay(200);
            
            if(isMuted==false){
              // Unmute the PGA2311
              digitalWrite(volumeMutePin,HIGH); 
              
              // Delay 
              delay(20);
              
              // Check the status of the channel switch 
              if(channelSwitch.update()){
                // Just set the flag to run through the channel logic
                channelRun=true;
              }
              else{
                // Otherwise scale the volume back up
                scaleVolume(0,channelVolume,50);
              }
           }
         }
      }  
   }
    
     
    /*
     * CHANNEL SELECT
     */
     
     // Only need to do this on switch state change
     if(channelSwitch.update() || channelRun){
       
       // Address the change
       if(channelSwitch.read()==(HIGH^channelSwitchReverse)){
         // Channel one
         if(selectedChannel==2){
            Serial.println("Switching to channel one...");
            selectedChannel=1;
       
            // Scale the volume down if it is not muted
            if(isMuted==false && channelRun==false){
              scaleVolume(channelVolume,0,35);
       
              // Mute the PGA2311
              digitalWrite(volumeMutePin,LOW); 
            }
            // Reset channelRun
            channelRun=false;
            
            // Load the volume value
            channelVolume=EEPROM.read(2*selectedOutput+selectedChannel);
            
            // Safegaurd for the maximum
            if(channelVolume>maximumVolume){
              channelVolume=maximumVolume;
            }
            tempChannelVolume=channelVolume;
            
            // Switch the channel relay
            digitalWrite(channelRelayPin,HIGH^channelRelayReverse);
            
            // Delay a bit for the relay to switch
            delay(200);
            
            if(isMuted==false){
              // Unmute the PGA2311
              digitalWrite(volumeMutePin,HIGH); 
              
              // Delay 
              delay(20);
              
              // Check the status of the output switch 
              if(outputSwitch.update()){
                // Just set the flag to run through the channel logic
                outputRun=true;
              }
              else{
                // Otherwise scale the volume back up
                scaleVolume(0,channelVolume,50);
              }
            }
         }
       }
    
       // Channel two
       else{
         if(selectedChannel==1){
            Serial.println("Switching to channel two...");
            selectedChannel=2;
        
            // Scale the volume down if it is not muted
            if(isMuted==false && channelRun==false){
              scaleVolume(channelVolume,0,35);
              
              // Mute the PGA2311
              digitalWrite(volumeMutePin,LOW); 
            }  
            // Reset channelRun
            channelRun=false;
              
            // Load the volume value
            channelVolume=EEPROM.read(2*selectedOutput+selectedChannel);
            
            // Safegaurd for the maximum
            if(channelVolume>maximumVolume){
              channelVolume=maximumVolume;
            }
            tempChannelVolume=channelVolume;
            
            // Switch the channel relay
            digitalWrite(channelRelayPin,LOW^channelRelayReverse);
            
            // delay a bit for the relay to switch
            delay(200);
            
            if(isMuted==false){
              // Unmute the PGA2311
              digitalWrite(volumeMutePin,HIGH); 
              
              // Delay 
              delay(20);
              
              // Check the status of the output switch 
              if(outputSwitch.update()){
                // Just set the flag to run through the channel logic
                outputRun=true;
              }
              else{
                // Otherwise scale the volume back up
                scaleVolume(0,channelVolume,50);
              }
            }
         }
      }  
   }
    
    
  /*
   * MUTE BUTTON
   */
  
  if(muteSwitch.update()){
     if(muteSwitch.read()==HIGH){
       if(isMuted==false){
         isMuted=true;
         Serial.println("Muting...");
         scaleVolume(channelVolume,0,35);
       }
       else{
         isMuted=false;
         Serial.println("Unmuting...");
         scaleVolume(0,channelVolume,50);
       }
     }
   }
  
  
 /*
  * VOLUME 
  */ 
  
  newVolumePosition=volumeKnob.read();
  if (newVolumePosition != 0) {
      
    // This section delays the encoder to make it feel better for user experience
      if(millis()-delayTimeout>5){
    
        // Unmute to last volume if the volume is changed
        if(isMuted==true){
          Serial.println("Unmuting..."); 
          scaleVolume(0,channelVolume,50);
        }
        isMuted=false;
      
        // Grab knob differential value and enforce volume bounds
        tempChannelVolume+=newVolumePosition;
        if(tempChannelVolume<0){
          tempChannelVolume=0;
        }
        if(tempChannelVolume>maximumVolume){
          tempChannelVolume=maximumVolume;
        }  
        
        channelVolume=tempChannelVolume;
        
        Serial.print("Volume = ");
        if(channelVolume==0){
          Serial.println("Mute");
        }
        else{
          // Save the volume setting
          EEPROM.write(2*selectedOutput+selectedChannel,channelVolume);
          
          // Print it out for debug
          Serial.print(31.5-((255-(float)channelVolume)/2));
          Serial.print(" dB (byte value = ");
          Serial.print(channelVolume);
          Serial.println(")");
        }
        
        // Reset the knob position
        volumeKnob.write(0);
        setVolume(channelVolume);
        delayTimeout=millis();  
    }
    else{
      volumeKnob.write(0);
    }
  }
}  
