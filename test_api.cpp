
#include <juce_gui_extra/juce_gui_extra.h>

void test() {
    juce::WebBrowserComponent* w = nullptr;
    w->emitEventIfBrowserIsVisible("test", juce::var());
}
