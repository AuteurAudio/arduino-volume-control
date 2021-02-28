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

// Maximum (byte value) of the volume to send to the PGA4311
// This is here to avoid regions where the high gains has too high S/N
// (192 is 0 dB -- e.g. no gain)
#define maximumVolume          192


/*
 * Includes, variables and some options
 */
 
 
//#define ENCODER_DO_NOT_USE_INTERRUPTS          // An option for the Encoder library
#include <Encoder.h>
#include <EEPROM.h>

 
// Encoder instantiation  
Encoder   volumeKnob(volumeKnobPin1, volumeKnobPin2);
long      newVolumePosition;
long      tempChannelVolume;
byte      channelVolume;
long int  delayTimeout;

// Other variables
int       isMuted =          false;            // Internally used status for mute state
int       channelRun =       false;            // Internally used status for passing control to channel 
int       outputRun =        false;            // Internally used status for passing control to output 


/*
 * Inline function to write a byte to the PGA4311
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
 * Function to set the (stereo) volume on the PGA4311
 */

void setVolume(long volume){
   byte base_vol=(byte)volume;
       
   digitalWrite(volumeSelectPin, LOW);   
   byteWrite(base_vol);
   byteWrite(base_vol);
   byteWrite(base_vol);
   byteWrite(base_vol);
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
  
  // Mute the PGA4311
  pinMode(volumeMutePin,OUTPUT);
  digitalWrite(volumeMutePin,LOW);           
  
  // Set up control pins for PGA4311
  pinMode(volumeSelectPin,OUTPUT);
  pinMode(volumeClockPin,OUTPUT);
  pinMode(volumeDataPin,OUTPUT);
  digitalWrite(volumeSelectPin,HIGH);
  digitalWrite(volumeClockPin,HIGH);
  digitalWrite(volumeDataPin,HIGH);
  
  // Delay a bit to wait for the state of the switches
  delay(100);
  
  // Set volume to 0 for later soft volume start
  setVolume(0);

  channelVolume=EEPROM.read(0);
  
  tempChannelVolume=channelVolume;
  
  Serial.print("Setting initial volume to (byte value) ");
  Serial.println(channelVolume);
  
  // Wait a bit for the whole system to come online
  delay(800);
  
  // Unmute the PGA4311
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
          EEPROM.write(0,channelVolume);
          
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
