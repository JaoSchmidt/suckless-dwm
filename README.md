## to do list for now

- Color all stuff
- Add special keyboard keys (volume, bright, etc)
- Remove gaps from first window (or maybe just decrease the gaps overall)
- Finish dwmblocks config
- add the rest of the patches
- make title apply only on float window
- float rule list
- make father windows floating
- replace st

### to do list maybe later

- understarnt wtf is systray
- add preview for tags
- 

## :exclamation: Patches and important corrections

- Every patch is available inside patches/ and statusbarPatches/
- Initial status bar [fix](https://www.reddit.com/r/suckless/comments/nfc3xn/gaps_problem_with_multiple_monitors/) was applied
- [Rezise hints was deactivated](https://dwm.suckless.org/faq/)
 

## :bulb: FontAwsome

### How to install FontAwesome
- Download the zip containing all fonts from the [link](https://fontawesome.com/download).
- Extract all `otf's` files inside ~/.local/share/fonts/. If you don't have the folder, create it.
- Use the following command: ```fc-cache -f -v```
- If conflict with some other font than restart will probably solve the problem

- on config.h overwrite the line:
  + ``` static const char *fonts[] = { "monospace:size=10" }; ```
- to
  + ``` static const char *fonts[] = { "monospace:size=10","fontawesome:size=10" }; ```


### Using it
- Choose a symbol [here](https://fontawesome.com/v5.15/icons?d=gallery&p=2&s=brands,solid&m=free).
- Copy its unicode, or use directly with ctrl+c and ctrl+v.
