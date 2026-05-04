// Version Frequency-Voltage
#include "mbed.h"
using namespace mbed;

#define PWM_PIN 9
#define MAX_TABLE_SIZE 100   // Maximum table size
int TABLE_SIZE = 50;        // Default starting table size
float amplitude = 1.0f;   // Fraction of Vdd (1.0 = Vdd, 0.5 = 0.5*Vdd, etc.)

PwmOut pwmPin(digitalPinToPinName(PWM_PIN));
Ticker sampleTicker;

float sineTable[MAX_TABLE_SIZE];

// ISR variables
volatile int idx = 0;
volatile int currentTableSize = 50;
volatile bool tickerAttached = false;

float frequency = 0.0f;
unsigned int currentIntervalUs = 0; // microseconds between samples

// ISR: output sine signal
void onSampleTick() {
    pwmPin.write(sineTable[idx]);
    idx++;
    if (idx >= currentTableSize) idx = 0;
}

// Build sine table for a given tableSize (values 0..1 for PWM)
void buildSineTable(int tableSize) {
    for (int i = 0; i < tableSize; i++) {
        sineTable[i] = amplitude * (sin(2.0f * M_PI * i / tableSize) + 1.0f) * 0.5f;
    }
}

// Attach or reconfigure ticker for the currentIntervalUs
void attachTickerUs(unsigned int intervalUs) {
    if (tickerAttached) {
        sampleTicker.detach();
        tickerAttached = false;
    }
    sampleTicker.attach_us(callback(&onSampleTick), intervalUs);
    tickerAttached = true;
}

// Update parameters for a given sine frequency
void updateFrequencyAndAmplitude(float freq, float amp) 
{
    if (freq <= 0.0f) 
        return;
    if (amp < 0.0f) 
        amp = 0.0f;
    if (amp > 1.0f) 
        amp = 1.0f;

    frequency = freq;
    amplitude = amp;

    // Adaptive table size for smoothness and CPU load
    if (freq <= 250.0f) currentTableSize = 100;
    else if (freq <= 500.0f) currentTableSize = 75;
    else if (freq <= 750.0f) currentTableSize = 50;
    else if (freq <= 1000.0f) currentTableSize = 30;
    else if (freq <= 1250.0f) currentTableSize = 20;
    else currentTableSize = 10;

    // Rebuild sine table using both new settings
    buildSineTable(currentTableSize);

    // Compute ISR interval (us) for desired frequency
    float interval_f = 1e6f / (frequency * (float)currentTableSize);
    if (interval_f < 10.0f) interval_f = 10.0f;
    currentIntervalUs = (unsigned int)(interval_f + 0.5f);

    idx = 0;
    attachTickerUs(currentIntervalUs);
}

void setup() {
    Serial.begin(115200);

    // High-frequency PWM carrier for better filtering
    pwmPin.period_us(15);  // Period (15-20 us gives the best results, too low makes it blocky)

    pwmPin.write(0.0f);

    // Initialize sine table
    currentTableSize = TABLE_SIZE;
    buildSineTable(currentTableSize);
}

void loop() {
    // Serial input: change frequency and voltage dynamically
    if (Serial.available()) {
        float newF = Serial.parseFloat();
        float newV = Serial.parseFloat();
        float newA = newV/3.3f; // Calculate amplitude

        // Clamping for edge cases
        if (newA < 0.0f) 
            newA = 0.0f;
        if (newA > 1.0f) 
            newA = 1.0f;
        newV = newA * 3.3f;

        if (newF > 0.0f) {
            updateFrequencyAndAmplitude(newF, newA);
            Serial.print("Frequency set to: ");
            Serial.println(newF);
            Serial.print("Voltage set to: ");
            Serial.println(newV); // Output voltage is a fraction of Vdd = 3.3V
        }
    }

    thread_sleep_for(10); // Small delay to free CPU
}