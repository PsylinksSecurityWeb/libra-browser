# Libra Browser Agent

The Agent is an optional helper inside Libra Browser. You give it a job in plain words ("fill this form from my notes", "summarise this page", "research this topic in a few tabs") and it works in the browser while you watch. It is **off by default**. While it is off, none of its code runs.

## Turn it on, hide it, configure it
- **Settings > AI Agent** is the control panel: switch the Agent on or off, hide its toolbar button, see the connection status, shorten requests, voice auto-send, and read what it can and cannot do.
- Hide the toolbar button any time (Settings > AI Agent > "Show the Agent button in the toolbar"). Ctrl+Shift+Space still opens it.
- **Connect your AI**: open the Agent, choose a provider (OpenAI, Anthropic Claude, Google Gemini, Groq, OpenRouter, Mistral, DeepSeek, xAI, Together, Perplexity, or a local Ollama), paste your own API key. The key is checked at once; a wrong key is refused and never kept. One key at a time.
- Inside the Agent panel (gear icon): provider and model, Memory (notes it may use), Files (the only files it may upload), Permissions (what you allowed and for how long), Voice.
- The Agent is **disabled in private windows and in Child Safety mode**.

## What it can do
- Open a website you name, read the page, click, type, choose from lists, tick boxes, press Enter/Tab/Escape.
- Fill forms from the notes and files you put in its **Memory**, and from facts you tell it in the chat (you can ask it to forget any of them).
- Read and summarise pages. Research: up to four extra tabs (read-only) per job, findings saved as untrusted "Web:" notes.
- Upload files, only from its own **Files** folder, using the site's normal upload button.
- Download a file, only after you say yes each time.
- Voice: speak your request. Speech is turned into text on this PC (whisper.cpp); no audio leaves the PC.
- Queue several jobs; they run one by one. Press Stop at any time.
- Polish typed prose (spelling, grammar) with the offline Windows spell checker; text you give word for word is left alone.

## What it never does
- Never types passwords, card numbers, security codes or one-time codes.
- Never solves CAPTCHAs or "are you human" checks. It stops completely and waits for you.
- Never searches the web by itself (it opens addresses or well-known sites you name).
- Never opens Libra Browser's own pages (Settings, Passwords) and never sees your saved passwords.
- Never reads files outside its Files folder; the AI sees file names only.
- Never opens local network or router addresses unless you named them.
- Never puts your saved details into a web address.

## You stay in control
Anything risky opens a Windows question first: submitting or sending, paying, leaving for another site, typing on a site you did not name, downloading. Choose Allow or Don't allow and how long to remember it (this time, 1 day, 1 week, 1 month, always for this site). Payment pages always ask. Before a form is sent, a review card lists every question and answer.

## Your data
- API key, Memory and permissions are encrypted with Windows (this user, this PC).
- Page text goes only to the provider you chose, with your own key. Nothing is sent to Psylinks.
- Nothing is saved in private windows.

## How it is protected (security model)
- A website can never talk to the Agent: web pages may only use a tiny password channel about their own address; every Agent command is accepted only from Libra's own Agent page.
- Websites cannot open or impersonate Libra pages (`libra://` navigation and popups are blocked); data meant for the Agent page is never sent into a page that came from the web.
- Page text is treated as data, never as orders. Hidden or pushy instructions on a page raise a warning instead of being obeyed. The Agent page shows all page and model text as plain text (no HTML).
- The AI can only ask for a fixed list of actions. Each one is checked by Libra before it runs: address rules, field rules (password and card fields refused), limits (steps, time, tabs, file choosers).
- A tricked or broken model cannot do more than the fixed list allows; wrong answers (not JSON, impossible ids, huge text) are rejected.

## Forms (investor and application forms)
Tested on a real Google Form (email, time, text, dropdown, date, checkboxes, radio buttons, paragraph), Typeform (one question per screen) and ordinary web forms with custom dropdowns. Put your details in **Memory** first (a notes file with your company, founders, numbers, links), then say: "Fill this form from my Memory". The Agent fills the fields, shows a **review card** with every question and answer, and only sends after you press Submit and say Allow in the Windows question.
- Forms embedded in another page (Typeform, HubSpot, Airtable, JotForm and similar, also from another site, also nested) are read and filled too. Ad frames are skipped. A CAPTCHA frame makes the Agent stop and wait for you.
- Still, the form's own link (docs.google.com/forms, typeform.com, airtable.com, tally.so) is the most reliable.
- It stops and asks you for passwords, card details, codes, CAPTCHAs and any answer it cannot find in your notes (it never invents one).
- Multi-page forms: it continues page by page; the last submit always needs your yes.
- File questions (pitch deck): put the file in the Agent's **Files** folder; it attaches by name.
- Not supported yet: fields hidden inside shadow DOM; sites that need you to sign in first (sign in yourself, then ask). Clicks inside frames are sent as page clicks, not real mouse clicks, so a few picky widgets may not react.
- If a page stops responding (stuck script, endless pop-ups) the Agent gives up after about 45 seconds and tells you.
- If it is going in circles (same action 4 times, 5 failed attempts in a row, or about 2.5 minutes without progress) it stops by itself with "I'm stuck" so no more tokens are spent. Tell it what to try next.

## When the assistant is closed, the AI sees nothing
The AI receives page content only during a task you typed into the open assistant panel. If you have not opened the assistant, or have closed it, nothing is read for it and nothing is sent to any AI provider. Closing the panel by any route (button, shortcut, tab close, turning the Agent off) stops a running task at once. This is tested: with the assistant never opened, 25 seconds of browsing sent 0 requests to the model and left nothing in the page.

## Auto suggest (no AI, this PC only)
Settings > AI Agent > Auto suggest (off by default). When you click an empty box for your name, e-mail, phone, company, website, address and similar, Libra shows up to three values from the notes you saved in the assistant's Memory, in its own small window under the box. Click a value and Libra types it. Or ignore it and type your own text.
- **No AI and no network.** The page tells Libra only which empty box you clicked (its label, name, type, size and position), never what is typed and never other page text. Matching happens on this PC against your notes (lines like "Email: ...", tables, CSV, JSON).
- **Only a real click or Tab key opens it**, like the autofill of Chrome, Firefox and Safari. A page that calls focus() itself, uses autofocus, or fakes events gets nothing. Libra also needs real keyboard or mouse use in the last few seconds.
- **Never** for passwords, user names, cards, bank details, ID numbers, date of birth, one-time codes or security answers, and never in a form that has a password box. Not in private windows, not in Child Safety mode, not on pages without https (except localhost), not while an assistant task runs.
- **The popup cannot leave the page**: it is placed inside the page area, hidden on scroll, resize, tab or window change, and a page cannot make it appear outside its own area.
- **You decide each time.** The text is typed only after you click a value, into the same box, if it is still empty and still the focused one. "Don't suggest on this site" is in the popup.
- Works best with notes written as "Name: value". Free paragraphs are not matched.

## Honest limits
An AI can make mistakes and some sites are built to confuse automation. Use the review card, keep Stop in reach, and do not use the Agent for money or legal decisions without checking its work. Local test suites for these rules live in `tools/tests` (not shipped in the app).
