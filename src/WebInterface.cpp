#include "WebInterface.h"

#include "config.h"
#include <WebServer.h>
#include <WiFi.h>

#include <math.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace
{
AnimatedText*   gAnimatedText  = nullptr;
AnimatedImage*  gAnimatedImage = nullptr;
WebServer       httpServer(80);

DisplayMode        gDisplayMode = DisplayMode::Text;
std::vector<Image> imageFrames;

constexpr float        kBrightnessGamma      = 2.2f;
constexpr uint16_t     kBrightnessFixedScale = 32;
uint16_t               gBrightnessDuty       = kBrightnessFixedScale;
uint8_t                gBrightnessPercent    = 100;

void updateBrightnessFromPercent(uint8_t percent)
{
    if (percent > 100)
        percent = 100;

    gBrightnessPercent = percent;

    if (percent == 0)
    {
        gBrightnessDuty = 0;
        return;
    }

    float normalized = static_cast<float>(percent) / 100.0f;
    float corrected  = powf(normalized, kBrightnessGamma);

    uint16_t duty = static_cast<uint16_t>(corrected * static_cast<float>(kBrightnessFixedScale) + 0.5f);
    if (duty == 0)
        duty = 1;
    if (duty > kBrightnessFixedScale)
        duty = kBrightnessFixedScale;

    gBrightnessDuty = duty;
}

String htmlEscape(const std::string& text)
{
    String escaped;
    escaped.reserve(text.size() * 2 + 16);
    for (char ch : text)
    {
        switch (ch)
        {
            case '&':  escaped += F("&amp;"); break;
            case '<':  escaped += F("&lt;"); break;
            case '>':  escaped += F("&gt;"); break;
            case '"':  escaped += F("&quot;"); break;
            case '\'': escaped += F("&#39;"); break;
            case '\n': escaped += F("&#10;"); break;
            case '\r': break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

String jsonEscape(const std::string& text)
{
    String escaped;
    escaped.reserve(text.size() * 2 + 16);
    for (unsigned char ch : text)
    {
        switch (ch)
        {
            case '\\': escaped += F("\\\\"); break;
            case '"':  escaped += F("\\\""); break;
            case '\n': escaped += F("\\n"); break;
            case '\r': escaped += F("\\r"); break;
            case '\t': escaped += F("\\t"); break;
            default:
                if (ch < 0x20)
                {
                    escaped += F("\\u00");
                    char buf[3];
                    snprintf(buf, sizeof(buf), "%02X", ch);
                    escaped += buf;
                }
                else
                {
                    escaped += static_cast<char>(ch);
                }
                break;
        }
    }
    return escaped;
}

AnimatedText::AnimationMode parseTextMode(const String& arg)
{
    if (arg.equalsIgnoreCase("scroll"))
    {
        return AnimatedText::AnimationMode::Scroll;
    }
    return AnimatedText::AnimationMode::Hold;
}

bool decodeHexFrame(const String& hex, Image& out)
{
    if (hex.length() != 64)
        return false;

    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    for (int row = 0; row < Image::kSize; ++row)
    {
        uint16_t value = 0;
        int base = row * 4;
        for (int j = 0; j < 4; j += 2)
        {
            int hi = nibble(hex[base + j]);
            int lo = nibble(hex[base + j + 1]);
            if (hi < 0 || lo < 0)
                return false;
            uint8_t byte = static_cast<uint8_t>((hi << 4) | lo);
            value = static_cast<uint16_t>((value << 8) | byte);
        }
        out.setRow(row, value);
    }
    return true;
}

String imageToHexString(const Image& image)
{
    String hex;
    hex.reserve(Image::kSize * 4);
    for (int row = 0; row < Image::kSize; ++row)
    {
        uint16_t value = image.getRow(row);
        char buf[5];
        snprintf(buf, sizeof(buf), "%04x", value);
        hex += buf;
    }
    return hex;
}

String buildStateJsonPayload()
{
    String payload;
    payload.reserve(256 + static_cast<int>(imageFrames.size()) * 68);
    const AnimatedText* textSource = gAnimatedText;
    std::string textValue          = textSource ? textSource->getText() : std::string(DEFAULT_INITIAL_TEXT);
    AnimatedText::AnimationMode textMode = textSource 
        ?  textSource->getAnimationMode()
        : (DEFAULT_TEXT_ANIMATION_MODE == 0 ? AnimatedText::AnimationMode::Hold : AnimatedText::AnimationMode::Scroll);
    uint32_t textFrameDuration = textSource 
        ?  textSource->getFrameDuration()
        : (textMode == AnimatedText::AnimationMode::Scroll ? DEFAULT_TEXT_FRAME_DURATION_LOOP_MS : DEFAULT_TEXT_FRAME_DURATION_HOLD_MS);

    const AnimatedImage* imageSource = gAnimatedImage;
    uint32_t imageFrameDuration      = imageSource ? imageSource->getFrameDuration() : DEFAULT_IMAGE_FRAME_DURATION_MS;
    bool     imageLoop               = imageSource ? imageSource->isLooping()        : DEFAULT_IMAGE_LOOPING;

    payload += F("{");
    payload += F("\"mode\":\"");
    payload += (gDisplayMode == DisplayMode::Text) ? F("text") : F("image");
    payload += F("\",");

    payload += F("\"text\":{");
    payload += F("\"value\":\"");
    payload += jsonEscape(textValue);
    payload += F("\",\"animation\":\"");
    payload += (textMode == AnimatedText::AnimationMode::Scroll) ? F("scroll") : F("hold");
    payload += F("\",\"frameDuration\":");
    payload += String(textFrameDuration);
    payload += F("},");

    payload += F("\"images\":{");
    payload += F("\"count\":");
    payload += String(static_cast<unsigned long>(imageFrames.size()));
    payload += F(",\"frameDuration\":");
    payload += String(imageFrameDuration);
    payload += F(",\"loop\":");
    payload += imageLoop ? F("true") : F("false");
    if (!imageFrames.empty())
    {
        payload += F(",\"firstFrame\":\"");
        payload += imageToHexString(imageFrames.front());
        payload += F("\"");
    }
    payload += F("}");
    payload += F(",\"brightness\":{");
    payload += F("\"percent\":");
    payload += String(gBrightnessPercent);
    payload += F(",\"duty\":");
    payload += String(static_cast<float>(gBrightnessDuty) / static_cast<float>(kBrightnessFixedScale), 4);
    payload += F(",\"scale\":");
    payload += String(kBrightnessFixedScale);
    payload += F("}");

    payload += F("}");
    Serial.println(payload);
    return payload;
}

String buildHtml()
{
    String page;
    page.reserve(4000);
    String stateJson = buildStateJsonPayload();
    page += F("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">");
    page += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    page += F("<title>LED Matrix Controller</title><style>");
    page += F("body{font-family:Segoe UI,Roboto,Helvetica,Arial,sans-serif;margin:1.6rem;background:#0b0b0b;color:#f4f4f4;}");
    page += F("h1{font-weight:600;margin-bottom:1.2rem;}section{margin-bottom:2rem;}");
    page += F(".card{background:#151515;border-radius:16px;padding:1.5rem;box-shadow:0 0 24px rgba(0,0,0,0.4);max-width:520px;}");
    page += F("label{display:block;margin-top:1rem;font-weight:600;}input[type=text],select,input[type=number]{width:100%;padding:0.65rem;border-radius:10px;border:1px solid #2b2b2b;background:#0e0e0e;color:#f4f4f4;margin-top:0.35rem;box-sizing:border-box;}");
    page += F("input[type=range]{width:100%;margin-top:0.35rem;}");
    page += F(".note{margin-top:0.8rem;font-size:0.85rem;color:#777;}");
    page += F("button{margin-top:1.2rem;border:none;border-radius:999px;padding:0.7rem 1.6rem;font-size:1rem;cursor:pointer;background:#0d6efd;color:#fff;transition:background 0.2s;}button:hover{background:#2680ff;}");
    page += F(".secondary{background:#333;} .secondary:hover{background:#3f3f3f;}");
    page += F("#status{margin-top:1rem;padding:0.8rem;border-radius:10px;background:#10253d;border:1px solid #1c4a7d;display:none;}");
    page += F(".grid{display:grid;gap:1.5rem;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));}");
    page += F(".preview-card{margin-top:1rem;}#imagePreview{width:160px;height:160px;border:1px solid #2b2b2b;border-radius:8px;background:#050505;image-rendering:pixelated;display:block;}#previewHint{font-size:0.85rem;color:#777;margin-top:0.35rem;}");
    page += F(".range-group{margin-top:1rem;}#imageThreshold{width:100%;} .range-scale{display:flex;justify-content:space-between;font-size:0.8rem;color:#666;margin-top:0.25rem;} .range-value{font-weight:600;color:#d0d0d0;}");
    page += F(".footer{margin-top:2rem;color:#888;}");
    page += F("</style></head><body><h1>LED Matrix Controller</h1><div class=\"grid\">");

    page += F("<section class=\"card\"><h2>Brightness</h2>");
    page += F("<div class=\"range-group\"><label for=\"brightnessRange\">Brightness</label><input id=\"brightnessRange\" type=\"range\" min=\"0\" max=\"100\" step=\"1\" value=\"100\"><div class=\"range-scale\"><span>0%</span><span id=\"brightnessValue\" class=\"range-value\">100%</span><span>100%</span></div></div>");
    page += F("<p class=\"note\">Gamma corrected frame skipping dims the display smoothly.</p>");
    page += F("</section>");
    page += F("<section class=\"card\"><h2>Text Animation</h2>");
    page += F("<form id=\"textForm\"><label for=\"textInput\">Text</label><input id=\"textInput\" name=\"text\" maxlength=\"64\" placeholder=\"Enter message\" required>");
    page += F("<label for=\"textMode\">Animation Mode</label><select id=\"textMode\" name=\"mode\"><option value=\"hold\">Hold (static)</option><option value=\"scroll\">Scroll</option></select>");
    page += F("<label for=\"textFrame\">Frame Duration (ms)</label><input type=\"number\" id=\"textFrame\" name=\"frameDuration\" min=\"0\" value=\"100\">");
    page += F("<button type=\"submit\">Update Text</button><button type=\"button\" class=\"secondary\" id=\"activateText\">Show Text</button></form></section>");

    page += F("<section class=\"card\"><h2>Image Animation</h2>");
    page += F("<label for=\"imageFiles\">Upload Images (any format)</label><input id=\"imageFiles\" type=\"file\" accept=\"image/*\" multiple>");
    page += F("<div class=\"range-group\"><label for=\"imageThreshold\">Binarization Threshold</label><input id=\"imageThreshold\" type=\"range\" min=\"0\" max=\"255\" value=\"128\"><div class=\"range-scale\"><span>0</span><span id=\"thresholdValue\" class=\"range-value\">128</span><span>255</span></div></div>");
    page += F("<label><input type=\"checkbox\" id=\"imageInvert\"> Invert output</label>");
    page += F("<label for=\"imageFrame\">Frame Duration (ms)</label><input type=\"number\" id=\"imageFrame\" min=\"0\" value=\"200\">");
    page += F("<label><input type=\"checkbox\" id=\"imageLoop\" checked> Loop playback</label>");
    page += F("<div class=\"preview-card\"><canvas id=\"imagePreview\" width=\"160\" height=\"160\"></canvas><div id=\"previewHint\">Preview shows the first selected image after conversion.</div></div>");
    page += F("<div><button type=\"button\" id=\"uploadImages\">Upload &amp; Replace Sequence</button><button type=\"button\" class=\"secondary\" id=\"activateImage\">Show Images</button></div>");
    page += F("<div id=\"imageSummary\" style=\"margin-top:0.8rem;color:#aaa;\"></div></section>");

    page += F("</div><div id=\"status\"></div><div class=\"footer\">Device IP: ");
    page += htmlEscape(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "(offline)");
    page += F("</div>");

    page += F("<script>");
    page += F("const initialState=");
    page += stateJson;
    page += F(";\n");
    page += F("const statusBox=document.getElementById('status');const brightnessRange=document.getElementById('brightnessRange');const brightnessValue=document.getElementById('brightnessValue');const textForm=document.getElementById('textForm');const textInput=document.getElementById('textInput');const textMode=document.getElementById('textMode');const textFrame=document.getElementById('textFrame');const activateTextBtn=document.getElementById('activateText');const imageFiles=document.getElementById('imageFiles');const imageFrame=document.getElementById('imageFrame');const imageLoop=document.getElementById('imageLoop');const uploadImagesBtn=document.getElementById('uploadImages');const activateImageBtn=document.getElementById('activateImage');const imageSummary=document.getElementById('imageSummary');const imageThreshold=document.getElementById('imageThreshold');const thresholdValue=document.getElementById('thresholdValue');const imageInvert=document.getElementById('imageInvert');const previewCanvas=document.getElementById('imagePreview');const previewCtx=previewCanvas.getContext('2d');const previewHint=document.getElementById('previewHint');\n");
    page += F("let brightnessUpdateTimer=null;\n");
    page += F("let previewSourceImage=null;\n");
    page += F("let currentState=initialState||null;\n");
    page += F("let currentDeviceFrameHex=currentState&&currentState.images?currentState.images.firstFrame||null:null;setBrightnessUI(currentState&&currentState.brightness?Number(currentState.brightness.percent):100);\n");
    page += F("function showStatus(msg,isError=false){statusBox.style.display='block';statusBox.textContent=msg;statusBox.style.background=isError?'#3d1010':'#10253d';statusBox.style.borderColor=isError?'#802525':'#1c4a7d';}\n");
    page += F("function updateBrightnessLabel(){const numeric=Number(brightnessRange.value);const clamped=Number.isFinite(numeric)?numeric:0;brightnessValue.textContent=Math.round(clamped)+'%';}\n");
    page += F("function setBrightnessUI(value){const numeric=Number(value);const clamped=Number.isFinite(numeric)?Math.min(Math.max(numeric,0),100):100;brightnessRange.value=clamped;brightnessValue.textContent=Math.round(clamped)+'%';}\n");
    page += F("async function postBrightness(value){try{const params=new URLSearchParams();params.set('value',value);const res=await fetch('/api/brightness',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:params});const data=await res.json();if(!res.ok||!data.ok)throw new Error(data.message||'Brightness update failed');if(data.brightness&&typeof data.brightness.percent==='number'){setBrightnessUI(Number(data.brightness.percent));}if(data.message){showStatus(data.message);}else{showStatus('Brightness updated');}}catch(err){console.error(err);showStatus(err.message||'Brightness update failed',true);}}\n");
    page += F("function scheduleBrightnessUpdate(immediate=false){if(brightnessUpdateTimer){clearTimeout(brightnessUpdateTimer);brightnessUpdateTimer=null;}if(immediate){postBrightness(brightnessRange.value);return;}brightnessUpdateTimer=setTimeout(()=>{brightnessUpdateTimer=null;postBrightness(brightnessRange.value);},150);}\n");
    page += F("function updateThresholdLabel(){thresholdValue.textContent=imageThreshold.value;}\n");
    page += F("function clearPreview(){previewCtx.fillStyle='#121212';previewCtx.fillRect(0,0,previewCanvas.width,previewCanvas.height);previewHint.textContent='Select an image to see the conversion preview.';}\n");
    page += F("function drawPreview(pixels){const scale=previewCanvas.width/16;previewCtx.fillStyle='#050505';previewCtx.fillRect(0,0,previewCanvas.width,previewCanvas.height);for(let y=0;y<16;y++){for(let x=0;x<16;x++){previewCtx.fillStyle=pixels[y][x]?'#00ffc8':'#1a1a1a';previewCtx.fillRect(x*scale,y*scale,scale,scale);}}}\n");
    page += F("function toHex(bytes){return Array.from(bytes).map(b=>b.toString(16).padStart(2,'0')).join('');}\n");
    page += F("function loadImageFromFile(file){return new Promise((resolve,reject)=>{const reader=new FileReader();reader.onload=()=>{const img=new Image();img.onload=()=>resolve(img);img.onerror=reject;img.src=reader.result;};reader.onerror=reject;reader.readAsDataURL(file);});}\n");
    page += F("const workingCanvas=document.createElement('canvas');workingCanvas.width=16;workingCanvas.height=16;const workingCtx=workingCanvas.getContext('2d');\n");
    page += F("function imageToFrameData(img,threshold,invert){workingCtx.clearRect(0,0,16,16);workingCtx.drawImage(img,0,0,16,16);const data=workingCtx.getImageData(0,0,16,16).data;const bytes=new Uint8Array(32);const pixels=Array.from({length:16},()=>Array(16).fill(0));for(let y=0;y<16;y++){let row=0;for(let x=0;x<16;x++){const idx=(y*16+x)*4;const a=data[idx+3];let lit=0;if(a>0){const r=data[idx];const g=data[idx+1];const b=data[idx+2];const lum=0.2126*r+0.7152*g+0.0722*b;lit=lum>threshold?1:0;}if(invert){lit=lit?0:1;}row=(row<<1)|lit;pixels[y][x]=lit;}bytes[y*2]=(row>>8)&0xFF;bytes[y*2+1]=row&0xFF;}return {bytes,pixels,hex:toHex(bytes)};}\n");
    page += F("async function fileToFrame(file,threshold,invert){const img=await loadImageFromFile(file);return {image:img,...imageToFrameData(img,threshold,invert)};}\n");
    page += F("function drawFramePreviewFromHex(hex){if(!hex||hex.length!==64){clearPreview();previewHint.textContent='No image loaded on device.';return;}const pixels=Array.from({length:16},()=>Array(16).fill(0));for(let y=0;y<16;y++){const row=parseInt(hex.slice(y*4,y*4+4),16);if(Number.isNaN(row)){clearPreview();return;}for(let x=0;x<16;x++){pixels[y][x]=(row>>(15-x))&1;}}drawPreview(pixels);previewHint.textContent='Preview shows current device frame.';}\n");
    page += F("function applyState(data,{preservePreview=false}={}){currentState=data||null;const brightnessPercent=currentState&&currentState.brightness?Number(currentState.brightness.percent):100;setBrightnessUI(brightnessPercent);currentDeviceFrameHex=currentState&&currentState.images?currentState.images.firstFrame||null:null;if(data&&data.text){textInput.value=data.text.value||'';textMode.value=data.text.animation||'hold';textFrame.value=data.text.frameDuration;}else{textInput.value='';textMode.value='hold';}if(data&&data.images){imageFrame.value=data.images.frameDuration;imageLoop.checked=!!data.images.loop;imageSummary.textContent=`${data.images.count} frame(s) loaded`;}else{imageSummary.textContent='0 frame(s) loaded';imageLoop.checked=false;}const isText=data?data.mode==='text':false;activateTextBtn.disabled=isText;activateImageBtn.disabled=data?data.mode==='image':false;if(!preservePreview){previewSourceImage=null;if(currentDeviceFrameHex){drawFramePreviewFromHex(currentDeviceFrameHex);}else{clearPreview();}}}\n");
    page += F("async function updatePreviewImage(){if(!previewSourceImage){if(currentDeviceFrameHex){drawFramePreviewFromHex(currentDeviceFrameHex);}else{clearPreview();}return;}const threshold=Number(imageThreshold.value);const invert=imageInvert.checked;const frame=imageToFrameData(previewSourceImage,threshold,invert);drawPreview(frame.pixels);previewHint.textContent='Preview shows local conversion (not yet uploaded).';}\n");
    page += F("async function handleFileSelection(){if(!imageFiles.files.length){previewSourceImage=null;if(currentDeviceFrameHex){drawFramePreviewFromHex(currentDeviceFrameHex);}else{clearPreview();}return;}try{previewSourceImage=await loadImageFromFile(imageFiles.files[0]);updatePreviewImage();}catch(err){console.error(err);showStatus('Preview failed',true);}}\n");
    page += F("async function refreshState(){try{const res=await fetch('/api/state');if(!res.ok)throw new Error('state load failed');const data=await res.json();const preservePreview=previewSourceImage!==null||imageFiles.files.length>0;applyState(data,{preservePreview});}catch(err){console.error(err);showStatus('Failed to refresh state',true);}}\n");
    page += F("async function uploadImages(){if(!imageFiles.files.length){showStatus('Please choose at least one image',true);return;}try{const threshold=Number(imageThreshold.value);const invert=imageInvert.checked;const frames=[];for(const file of imageFiles.files){const frameData=await fileToFrame(file,threshold,invert);frames.push(frameData.hex);}const params=new URLSearchParams();params.set('frames',frames.join(','));params.set('frameDuration',imageFrame.value||'0');params.set('loop',imageLoop.checked?'1':'0');const res=await fetch('/api/images',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:params});const data=await res.json();if(!res.ok||!data.ok)throw new Error(data.message||'Upload failed');showStatus(data.message);refreshState();}catch(err){console.error(err);showStatus(err.message||'Upload failed',true);}}\n");
    page += F("uploadImagesBtn.addEventListener('click',uploadImages);\n");
    page += F("textForm.addEventListener('submit',async e=>{e.preventDefault();try{const params=new URLSearchParams();params.set('text',textInput.value);params.set('mode',textMode.value);params.set('frameDuration',textFrame.value||'0');const res=await fetch('/api/text',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:params});const data=await res.json();if(!res.ok||!data.ok)throw new Error(data.message||'Update failed');showStatus(data.message);refreshState();}catch(err){console.error(err);showStatus(err.message||'Update failed',true);}});\n");
    page += F("activateTextBtn.addEventListener('click',async ()=>{const params=new URLSearchParams();params.set('mode','text');const res=await fetch('/api/mode',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:params});const data=await res.json();if(!res.ok||!data.ok){showStatus(data.message||'Mode change failed',true);}else{showStatus(data.message);refreshState();}});\n");
    page += F("activateImageBtn.addEventListener('click',async ()=>{const params=new URLSearchParams();params.set('mode','image');const res=await fetch('/api/mode',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:params});const data=await res.json();if(!res.ok||!data.ok){showStatus(data.message||'Mode change failed',true);}else{showStatus(data.message);refreshState();}});\n");
    page += F("brightnessRange.addEventListener('input',()=>{updateBrightnessLabel();scheduleBrightnessUpdate(false);});brightnessRange.addEventListener('change',()=>{updateBrightnessLabel();scheduleBrightnessUpdate(true);});imageFiles.addEventListener('change',handleFileSelection);imageThreshold.addEventListener('input',()=>{updateThresholdLabel();updatePreviewImage();});imageInvert.addEventListener('change',updatePreviewImage);\n");
    page += F("updateThresholdLabel();\n");
    page += F("applyState(initialState,{preservePreview:false});\n");
    page += F("refreshState();</script></body></html>");
    return page;
}

void sendJsonResponse(int code, bool ok, const String& message)
{
    String payload;
    payload.reserve(64 + message.length());
    payload += F("{\"ok\":");
    payload += ok ? F("true") : F("false");
    payload += F(",\"message\":\"");
    payload += jsonEscape(std::string(message.c_str()));
    payload += F("\"}");
    httpServer.send(code, "application/json", payload);
    Serial.print(F("[Web] JSON response: "));
    Serial.println(payload);
}

void sendStateJson()
{
    String payload = buildStateJsonPayload();
    httpServer.send(200, "application/json", payload);
    Serial.print(F("[Web] JSON response: "));
    Serial.println(payload);
}

void applyDisplayMode(DisplayMode mode)
{
    if (gAnimatedText == nullptr || gAnimatedImage == nullptr)
        return;

    gDisplayMode = mode;
}

void handleRoot()
{
    httpServer.send(200, "text/html", buildHtml());
}

void handleApiState()
{
    sendStateJson();
}

void handleApiText()
{
    if (!gAnimatedText)
    {
        sendJsonResponse(400, false, F("Text component not initialized"));
        return;
    }  

    if (!httpServer.hasArg("text"))
    {
        sendJsonResponse(400, false, F("Missing text parameter"));
        return;
    }

    if (httpServer.hasArg("mode"))
    {
        AnimatedText::AnimationMode modeArg = parseTextMode(httpServer.arg("mode"));
        gAnimatedText->setAnimationMode(modeArg);
        gAnimatedText->setFrameDuration(modeArg == AnimatedText::AnimationMode::Hold
            ? DEFAULT_TEXT_FRAME_DURATION_HOLD_MS
            : DEFAULT_TEXT_FRAME_DURATION_LOOP_MS);
    }

    if (httpServer.hasArg("frameDuration"))
    {
        int frameDuration = httpServer.arg("frameDuration").toInt();
        if (frameDuration < 0)
            frameDuration = 0;

        gAnimatedText->setFrameDuration(frameDuration);
    }

    gAnimatedText->setText(httpServer.arg("text").c_str());
    gAnimatedText->reset();

    sendJsonResponse(200, true, F("Text updated"));
}

void handleApiImages()
{
    if (!gAnimatedImage)
    {
        sendJsonResponse(400, false, F("Image component not initialized"));
        return;
    }  


    if (!httpServer.hasArg("frames"))
    {
        sendJsonResponse(400, false, F("Missing frames parameter"));
        return;
    }

    String framesArg = httpServer.arg("frames");
    framesArg.trim();

    if (framesArg.length() == 0)
    {
        imageFrames.clear();
        gAnimatedImage->clearFrames();

        sendJsonResponse(200, true, F("Frames cleared"));
        return;
    }

    std::vector<Image> newFrames;
    int start = 0;
    while (start < framesArg.length())
    {
        int comma = framesArg.indexOf(',', start);
        String token = (comma == -1) ? framesArg.substring(start) : framesArg.substring(start, comma);
        token.trim();
        if (token.length() == 0)
        {
            start = (comma == -1) ? framesArg.length() : comma + 1;
            continue;
        }
        Image img;
        if (!decodeHexFrame(token, img))
        {
            sendJsonResponse(400, false, F("Invalid frame data"));
            return;
        }
        newFrames.push_back(img);
        if (comma == -1)
        {
            break;
        }
        start = comma + 1;
    }

    if (newFrames.empty())
    {
        sendJsonResponse(400, false, F("No valid frames provided"));
        return;
    }

    imageFrames = std::move(newFrames);
    gAnimatedImage->setFrames(imageFrames);

    if (httpServer.hasArg("frameDuration"))
    {
        long frameDuration = httpServer.arg("frameDuration").toInt();
        if (frameDuration < 0)
            frameDuration = 0;

        gAnimatedImage->setFrameDuration(frameDuration);
    }

    if (httpServer.hasArg("loop"))
    {
        bool looping = (httpServer.arg("loop").toInt() != 0);
        gAnimatedImage->setLooping(looping);

    }

    gAnimatedImage->reset();
    sendJsonResponse(200, true, F("Image sequence updated"));
}

void handleApiBrightness()
{
    if (!httpServer.hasArg("value"))
    {
        sendJsonResponse(400, false, F("Missing value parameter"));
        return;
    }

    float percent = httpServer.arg("value").toFloat();
    if (percent < 0.0f)
        percent = 0.0f;
    if (percent > 100.0f)
        percent = 100.0f;

    updateBrightnessFromPercent(static_cast<uint8_t>(percent + 0.5f));

    String message = F("Brightness set to ");
    message += String(static_cast<int>(gBrightnessPercent));
    message += F("%");

    String payload;
    payload.reserve(128);
    payload += F("{\"ok\":true,\"message\":\"");
    payload += jsonEscape(std::string(message.c_str()));
    payload += F("\",\"brightness\":{");
    payload += F("\"percent\":");
    payload += String(static_cast<int>(gBrightnessPercent));
    payload += F(",\"duty\":");
    payload += String(static_cast<float>(gBrightnessDuty) / static_cast<float>(kBrightnessFixedScale), 4);
    payload += F(",\"scale\":");
    payload += String(kBrightnessFixedScale);
    payload += F("}}");

    httpServer.send(200, "application/json", payload);
    Serial.print(F("[Web] JSON response: "));
    Serial.println(payload);
}

void handleApiMode()
{
    if (!httpServer.hasArg("mode"))
    {
        sendJsonResponse(400, false, F("Missing mode parameter"));
        return;
    }

    String modeArg = httpServer.arg("mode");
    modeArg.toLowerCase();
    if (modeArg == "image")
    {
        applyDisplayMode(DisplayMode::Image);
        sendJsonResponse(200, true, F("Switched to image animation"));
    }
    else
    {
        applyDisplayMode(DisplayMode::Text);
        sendJsonResponse(200, true, F("Switched to text animation"));
    }
}

void handleNotFound()
{
    httpServer.send(404, "text/plain", "Not Found");
}

} // namespace

void WebInterface_begin(AnimatedText& animatedText,
                        AnimatedImage& animatedImage,
                        const char* wifiSsid,
                        const char* wifiPassword,
                        const char* wifiHostname)
{
    gAnimatedText  = &animatedText;
    gAnimatedImage = &animatedImage;

    updateBrightnessFromPercent(gBrightnessPercent);

    imageFrames.clear();

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    if (wifiHostname != nullptr && wifiHostname[0] != '\0')
    {
        WiFi.setHostname(wifiHostname);
    }

    if (wifiSsid != nullptr && wifiSsid[0] != '\0')
    {
        WiFi.begin(wifiSsid, wifiPassword != nullptr ? wifiPassword : "");
        Serial.print(F("Connecting to WiFi"));
        uint32_t startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000)
        {
            Serial.print('.');
            delay(500);
        }
        Serial.println();
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.print(F("Connected. IP address: "));
            Serial.println(WiFi.localIP());
        }
        else
        {
            Serial.println(F("WiFi connection failed (continuing offline)."));
        }
    }
    else
    {
        Serial.println(F("WiFi SSID not provided; running without network."));
    }

    httpServer.on("/", handleRoot);
    httpServer.on("/api/state", HTTP_GET, handleApiState);
    httpServer.on("/api/text", HTTP_POST, handleApiText);
    httpServer.on("/api/images", HTTP_POST, handleApiImages);
    httpServer.on("/api/brightness", HTTP_POST, handleApiBrightness);
    httpServer.on("/api/mode", HTTP_POST, handleApiMode);
    httpServer.onNotFound(handleNotFound);
    httpServer.begin();

    Serial.print(F("HTTP server started. Open http://"));
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println(F("device-local"));
    }
}

void WebInterface_handle()
{
    httpServer.handleClient();
}

DisplayMode WebInterface_getDisplayMode()
{
    return gDisplayMode;
}

uint16_t WebInterface_getBrightnessDuty()
{
    return gBrightnessDuty;
}

uint16_t WebInterface_getBrightnessScale()
{
    return kBrightnessFixedScale;
}


