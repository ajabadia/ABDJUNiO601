/**
 * wasm_link_stubs.cpp — Mocks and Linker Stubs for JUCE WASM build.
 *
 * Provides minimal implementations for JUCE symbols that are referenced
 * by compiled modules but not needed at runtime in a headless WASM build.
 */

#include <JuceHeader.h>

namespace juce
{

// Stub for juce::File::getSpecialLocation (needed by juce_core internals)
File File::getSpecialLocation(const SpecialLocationType)
{
    return File();
}

// Stub for MessageManager::postMessageToSystemQueue
#if JUCE_MODULE_AVAILABLE_juce_events
bool MessageManager::postMessageToSystemQueue(MessageManager::MessageBase* message)
{
    delete message;
    return true;
}
#endif

} // namespace juce
