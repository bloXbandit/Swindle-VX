#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cstring>
#include <string>

#ifndef VOCALSUITE_EMBED_UI
 #define VOCALSUITE_EMBED_UI 0
#endif

#if VOCALSUITE_EMBED_UI
 #include "BinaryData.h"
#endif

VocalSuiteAudioProcessorEditor::VocalSuiteAudioProcessorEditor(VocalSuiteAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), 
      webView(juce::WebBrowserComponent::Options()
                .withResourceProvider([this](const auto& url) { return getResource(url); })
                .withNativeIntegrationEnabled()
                .withEventListener("parameterChange", [this](const auto& object) { handleMessage(object); })
                .withEventListener("loadModel", [this](const auto& object) { handleMessage(object); })
                .withEventListener("startCapture", [this](const auto& object) { handleMessage(object); })
                .withEventListener("stopCapture", [this](const auto& object) { handleMessage(object); })
                .withEventListener("convertAudio", [this](const auto& object) { handleMessage(object); }))
{
    addAndMakeVisible(webView);
    
    webView.goToURL("http://localhost:5173");
    
    setSize(1000, 700);
}

VocalSuiteAudioProcessorEditor::~VocalSuiteAudioProcessorEditor() {}

void VocalSuiteAudioProcessorEditor::paint(juce::Graphics& g) {}

void VocalSuiteAudioProcessorEditor::resized() {
    webView.setBounds(getLocalBounds());
}

void VocalSuiteAudioProcessorEditor::handleMessage(const juce::var& message)
{
    if (message.isObject())
    {
        auto* obj = message.getDynamicObject();
        auto type = obj->getProperty("type").toString();

        if (type == "parameterChange")
        {
            if (!obj->hasProperty("name") || !obj->hasProperty("value"))
                return;

            auto name = obj->getProperty("name").toString();
            auto valueVar = obj->getProperty("value");
            if (!valueVar.isDouble() && !valueVar.isInt() && !valueVar.isInt64())
                return;

            auto value = (float) valueVar;

            if (auto* param = audioProcessor.parameters.getParameter(name))
            {
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
                    param->setValueNotifyingHost(ranged->convertTo0to1(value));
                else
                    param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, value));
            }
        }
        else if (type == "loadModel")
        {
            if (!obj->hasProperty("modelId") || !obj->hasProperty("modelType"))
                return;

            auto modelId = obj->getProperty("modelId").toString();
            auto modelType = obj->getProperty("modelType").toString();

            audioProcessor.loadVoiceModel(modelId.toStdString(), modelType.toStdString());
        }
        else if (type == "startCapture")
        {
            audioProcessor.startCapture();
        }
        else if (type == "stopCapture")
        {
            audioProcessor.stopCapture();
        }
        else if (type == "convertAudio")
        {
            if (!obj->hasProperty("model"))
                return;

            auto model = obj->getProperty("model").toString();

            int pitchShift = 0;
            float formantShift = 0.0f;

            if (obj->hasProperty("pitchShift"))
                pitchShift = (int) obj->getProperty("pitchShift");
            if (obj->hasProperty("formantShift"))
                formantShift = (float) obj->getProperty("formantShift");

            audioProcessor.convertCapturedAudio(model.toStdString(), pitchShift, formantShift);
        }
    }
}

std::optional<juce::WebBrowserComponent::Resource> VocalSuiteAudioProcessorEditor::getResource(const juce::String& url) {
   #if !VOCALSUITE_EMBED_UI
    (void) url;
    return std::nullopt;
   #else
    auto path = url;

    path = path.upToFirstOccurrenceOf("?", false, false);
    path = path.upToFirstOccurrenceOf("#", false, false);

    while (path.startsWithChar('/'))
        path = path.substring(1);

    if (path.isEmpty())
        path = "index.html";

    if (path.endsWithChar('/'))
        path += "index.html";

    // Vite build outputs assets under "assets/" but juce_add_binary_data flattens paths.
    auto flatPath = path;
    if (flatPath.startsWithIgnoreCase("assets/"))
        flatPath = flatPath.fromFirstOccurrenceOf("assets/", false, true);

    auto resourceName = flatPath.replaceCharacter('/', '_')
                                .replaceCharacter('-', 0)
                                .replaceCharacter('.', '_');

    int dataSize = 0;
    const char* data = BinaryData::getNamedResource(resourceName.toRawUTF8(), dataSize);

    if ((data == nullptr || dataSize <= 0) && path.equalsIgnoreCase("index.html")) {
        data = BinaryData::getNamedResource("index_html", dataSize);
    }

    if (data == nullptr || dataSize <= 0) {
        juce::String debugInfo = "<!DOCTYPE html><html><body style='font-family:monospace;padding:20px;background:#000;color:#f00;'>";
        debugInfo += "<h2>Resource Not Found</h2>";
        debugInfo += "<p>Requested URL: " + url + "</p>";
        debugInfo += "<p>Path: " + path + "</p>";
        debugInfo += "<p>Flat path: " + flatPath + "</p>";
        debugInfo += "<p>Resource name: " + resourceName + "</p>";
        debugInfo += "<p>Available in BinaryData:</p><ul>";
        debugInfo += "<li>index_html</li>";
        debugInfo += "<li>indexCeMAjhD_css</li>";
        debugInfo += "<li>indexDjnBBgLm_js</li>";
        debugInfo += "<li>favicon_svg</li>";
        debugInfo += "<li>vite_svg</li>";
        debugInfo += "<li>_redirects</li>";
        debugInfo += "</ul></body></html>";
        
        juce::WebBrowserComponent::Resource debugRes;
        debugRes.mimeType = "text/html";
        debugRes.data.resize(debugInfo.length());
        std::memcpy(debugRes.data.data(), debugInfo.toRawUTF8(), debugInfo.length());
        return debugRes;
    }

    juce::String mimeType = "application/octet-stream";
    if (path.endsWithIgnoreCase(".html")) mimeType = "text/html";
    else if (path.endsWithIgnoreCase(".js")) mimeType = "text/javascript";
    else if (path.endsWithIgnoreCase(".css")) mimeType = "text/css";
    else if (path.endsWithIgnoreCase(".svg")) mimeType = "image/svg+xml";
    else if (path.endsWithIgnoreCase(".png")) mimeType = "image/png";
    else if (path.endsWithIgnoreCase(".jpg") || path.endsWithIgnoreCase(".jpeg")) mimeType = "image/jpeg";
    else if (path.endsWithIgnoreCase(".woff")) mimeType = "font/woff";
    else if (path.endsWithIgnoreCase(".woff2")) mimeType = "font/woff2";
    else if (path.endsWithIgnoreCase(".json")) mimeType = "application/json";

    juce::WebBrowserComponent::Resource res;
    res.mimeType = mimeType;

    if (path.equalsIgnoreCase("index.html") && data != nullptr && dataSize > 0)
    {
        std::string html(data, data + dataSize);

        const std::string needle = "<script src=\"https://blink.new/auto-engineer.js?projectId=vocal-suite-plugin-k660u1pj\" type=\"module\"></script>";
        if (auto pos = html.find(needle); pos != std::string::npos)
            html.erase(pos, needle.size());

        const std::string errorLogger = 
            "<script>\n"
            "(function(){\n"
            "var errors=[];\n"
            "var overlay=null;\n"
            "function show(){\n"
            "if(!overlay){\n"
            "overlay=document.createElement('div');\n"
            "overlay.style.cssText='position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.95);color:#f00;font-family:monospace;font-size:12px;padding:20px;overflow:auto;z-index:999999';\n"
            "document.body.appendChild(overlay);\n"
            "}\n"
            "overlay.innerHTML='<h2>JS Errors:</h2>'+errors.map(function(e){return '<p>'+e+'</p>';}).join('');\n"
            "}\n"
            "window.addEventListener('error',function(e){\n"
            "errors.push('ERROR: '+e.message+' at '+e.filename+':'+e.lineno);\n"
            "show();\n"
            "});\n"
            "window.addEventListener('unhandledrejection',function(e){\n"
            "errors.push('PROMISE: '+e.reason);\n"
            "show();\n"
            "});\n"
            "})();\n"
            "</script>\n";

        auto insertPos = html.find("</head>");
        if (insertPos != std::string::npos)
            html.insert(insertPos, errorLogger);

        res.data.resize(html.size());
        std::memcpy(res.data.data(), html.data(), html.size());
        return res;
    }

    res.data.resize((size_t) dataSize);
    std::memcpy(res.data.data(), data, (size_t) dataSize);
    return res;
   #endif
}