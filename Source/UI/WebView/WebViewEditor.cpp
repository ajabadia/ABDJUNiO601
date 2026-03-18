#include "WebViewEditor.h"
#include "../../Core/PresetManager.h"
#include <BinaryData.h>

WebViewEditor::WebViewEditor (SimpleJuno106AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), lastLibraryIndex(-1)
{
    auto resolveLogDir = []() -> juce::File {
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("JUNiO601").getChildFile("logs").getChildFile("webview_data");
    };

    const auto getMimeType = [] (const juce::String& path) -> juce::String
    {
        if (path.endsWith (".html")) return "text/html";
        if (path.endsWith (".css"))  return "text/css";
        if (path.endsWith (".js"))   return "text/javascript";
        if (path.endsWith (".png"))  return "image/png";
        if (path.endsWith (".ttf"))  return "font/ttf";
        if (path.endsWith (".woff")) return "font/woff";
        if (path.endsWith (".woff2")) return "font/woff2";
        if (path.endsWith (".svg"))  return "image/svg+xml";
        return "application/octet-stream";
    };

    // Setup WebView with Options
    auto options = juce::WebBrowserComponent::Options()
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2()
                                  .withUserDataFolder (resolveLogDir())
                                  .withBackgroundColour (juce::Colour (0xff111111))) // Windows specific background
        .withNativeIntegrationEnabled()
        .withResourceProvider ([getMimeType] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource> 
        {
            auto path = url == "/" ? "index.html" : url.substring (1);
            juce::String filename = juce::File (path).getFileName();
            
            // Robust lookup: iterate through BinaryData to find the original filename
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            {
                if (filename == BinaryData::originalFilenames[i])
                {
                    int dataSize = 0;
                    if (const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], dataSize))
                    {
                        std::vector<std::byte> dataVec (static_cast<size_t> (dataSize));
                        std::memcpy (dataVec.data(), data, static_cast<size_t> (dataSize));
                        return juce::WebBrowserComponent::Resource { std::move (dataVec), getMimeType (path) };
                    }
                }
            }

            juce::Logger::writeToLog ("WebView resource NOT FOUND in BinaryData: " + path + " (looked for filename: " + filename + ")");
            return std::nullopt;
        });
// ... [rest of file remains the same as in previous commit, but with the specific line removed]
