#pragma once

#include <JuceHeader.h>
#include <mutex>

/**
 * JunoLogger
 * 
 * Provides structured logging for the JUNiO 601.
 * Supports categories and severity levels.
 */
namespace JunoLogger {

enum class Level {
    Debug,
    Info,
    Warning,
    Error
};

inline juce::String levelToString(Level level) {
    switch (level) {
        case Level::Debug:   return "DEBUG";
        case Level::Info:    return "INFO";
        case Level::Warning: return "WARNING";
        case Level::Error:   return "ERROR";
    }
    return "UNKNOWN";
}

/**
 * Log a message to the debug output and potentially a file.
 */
inline void log(Level level, const juce::String& category, const juce::String& message) {
    juce::String formattedMessage = juce::String::formatted("[%s] [%s] %s", 
                                    levelToString(level).toRawUTF8(), 
                                    category.toRawUTF8(), 
                                    message.toRawUTF8());
    
    // Always output to debug console
    juce::Logger::writeToLog(formattedMessage);
    
    // [Future] Could also write to a persistent log file here 
    // using a thread-safe mechanism if requested.
}

// Convenience methods
inline void debug(const juce::String& category, const juce::String& message) { log(Level::Debug, category, message); }
inline void info(const juce::String& category, const juce::String& message)  { log(Level::Info, category, message); }
inline void warn(const juce::String& category, const juce::String& message)  { log(Level::Warning, category, message); }
inline void error(const juce::String& category, const juce::String& message) { log(Level::Error, category, message); }

} // namespace JunoLogger
