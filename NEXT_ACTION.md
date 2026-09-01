# Next Action

## Current
1. ✅ Created Linux feature branch: `feature/tasia-giphy-welcome-loading-linux`.
2. ✅ Added generated obfuscated GIPHY key fallback infrastructure.
3. ✅ Added Tasia welcome/GIPHY/loading settings.
4. ✅ Implemented async welcome.txt random-line fetch and hooked it into `LLProgressView`.
5. ✅ Implemented `LLGiphyClient` search/trending support and no-key fallback.
6. ✅ Implemented and registered `LLFloaterGiphyPicker` with search/trending list UI and `Powered by GIPHY` footer.
7. ✅ Added nearby chat `GIF` button and wired picker selection to send the selected normal GIPHY page URL.
8. ✅ Added local GIPHY URL preview cards in chat history behind `TasiaAnimatedGifChatPreview`.
9. ✅ Added direct image URL previews in chat/IM history behind `TasiaImageChatPreview`.
10. ✅ Added YouTube embeds in chat/IM history behind `TasiaYouTubeChatPreview` enabled by default.
11. ✅ Added loading panel Tasia branding/GIPHY credit and optional loading YouTube media, disabled by default.
12. ✅ Verified active nearby chat and Firestorm IM use `FSChatHistory`; legacy `LLChatHistory` path is disabled by `#if 0`.
13. ✅ Ran focused generator/XML/whitespace verification.
14. **Before GitHub Linux build, ensure `FMOD_DEPS_TOKEN` exists and add `TASIA_GIPHY_API_KEY` if GIPHY should work out of box.** ← NOW

## Blockers
- None currently.
