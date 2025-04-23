# Knowbridge

Knowbridge is a KDE Plasma tool that uses LLMs to edit text directly inside any application. Select text, use a shortcut, and it modifies the text in place using AI.

![](docs/demo.gif)

## ✨ Key Features

*   **Global Activation:** Triggered by a configurable global key combination from any application.
*   **Contextual Text Retrieval:** Uses AT-SPI to get selected text (or all text) from the active application window.
*   **In-Place Text Replacement:** Utilizes the AT-SPI (`EditableText` interface) to directly replace the original text with the LLM's output in compatible applications.
*   **Actions Menu:** Displays a context menu near the mouse cursor with predefined actions (Fix Grammar, Improve Style, Make Formal, etc.).
*   **Support for OpenAI API:** Sends requests to any OpenAI-compatible API endpoint (defaults to `http://localhost:30000/v1/chat/completions`, great for local LLMs like Ollama, llama.cpp).
*   **Clipboard Fallback:** If AT-SPI is unavailable or the application doesn't support editing via AT-SPI, the utility retrieves text from the selection clipboard (or primary clipboard) and copies the result back to the clipboard.
*   **Notifications:** Uses KDE system notifications (`KNotification`) to inform about status (processing, success, error, copied to clipboard).
*   **KDE Integration:** Built with Qt 6 and KDE Frameworks 6, integrates seamlessly with the Plasma desktop. Shortcut configuration is available via "System Settings" -> "Shortcuts".
*   **System Tray Icon:** Displays an icon for indication of running status and (in the future) quick access to settings/quitting.

## ⚙️ How It Works

1.  The user presses the global key combination (default is `Ctrl+Alt+Space`).
2.  The `BackgroundProcessor` intercepts the signal.
3.  The `AccessibilityHelper` attempts to identify the active window and the focused text element using AT-SPI.
    *   **If successful and the element is editable:** Retrieves the selected text (or all text if nothing is selected). Stores element information (`ElementInfo`), including a pointer to the `AtspiAccessible`.
    *   **If AT-SPI fails or the element is not editable:** Tries to get text from the selection clipboard (`QClipboard::Selection`), then the primary clipboard (`QClipboard::Clipboard`).
4.  If text is found (either via AT-SPI or clipboard), a `QMenu` with the list of available actions (`TextAction`) appears at the mouse cursor.
5.  The user selects an action.
6.  The `BackgroundProcessor` calls the `ApiClient` to send the text and the selected prompt to the configured LLM API endpoint.
7.  The `ApiClient` sends a POST request and waits asynchronously for the response.
8.  Upon receiving the LLM response:
    *   **If the original text was obtained via AT-SPI and the element was editable:** The `AccessibilityHelper` attempts to use the `AtspiEditableText` interface to delete the original text and insert the new text into the application.
    *   **If AT-SPI replacement failed or was not possible (e.g., clipboard was used):** The `BackgroundProcessor` copies the result to the primary clipboard (`QClipboard::Clipboard`).
9.  The user is shown a notification regarding the status (in-place replacement success, copied to clipboard, or error).

## 🚀 Requirements

### For Running:

*   **Operating System:** Linux with KDE Plasma 6 desktop.
*   **Environment:** A running AT-SPI service (`at-spi2-core`, usually enabled by default in modern distributions).
*   **API Endpoint:** An accessible OpenAI-compatible API endpoint (e.g., a locally running Ollama instance with `ollama serve`, or another server).
*   **API Key:** The environment variable `OPENAI_API_KEY` must be set (even if your local LLM doesn't require a key, the API client expects this variable; you can set it to any non-empty value, e.g., `export OPENAI_API_KEY=dummy`).

> Note: It can work with Xorg/Wayland. However, support for Wayland is limited and requires refinement.

### For Building:

*   **CMake:** Version 3.20 or higher.
*   **Compiler:** C++17 compliant (GCC or Clang).
*   **Qt 6:** Core libraries and development tools (version 6.6.0 or higher).
    *   `qt6-base`, `qt6-tools` (or equivalents in your distribution).
*   **KDE Frameworks 6:** Libraries and development tools (version 6.0.0 or higher).
    *   `extra-cmake-modules`
    *   `kcoreaddons-devel`, `kglobalaccel-devel`, `ki18n-devel`, `kxmlgui-devel`, `knotifications-devel` (package names may vary, look for `-devel` or `-dev` suffixes).
*   **AT-SPI Dependencies:** Libraries and header files for AT-SPI and its dependencies.
    *   `at-spi2-core-devel` (or `libatk-bridge2.0-dev`)
    *   `atk-devel` (or `libatk1.0-dev`)
    *   `glib2-devel` (or `libglib2.0-dev`)
    *   `gobject-introspection-devel` (might be needed for pkg-config)
*   **pkg-config:** Utility to find libraries.

## 🔧 Installation and Building
`Dockerfile` and `docker-compose.yml` are provided for building in an isolated Manjaro environment. The result is copied to the `dist` folder. **Remember to apply all file name and code changes before building in Docker.**
```bash
# Ensure project files are updated to 'Knowbridge' / 'knowbridge'
docker compose build
docker compose run --rm kde-llm-enhancer # Builds and copies artifacts to ./dist
# Then you can install from ./dist
```

## ⚙️ Configuration

*   **API Endpoint and Model:** Currently, the URL (`http://localhost:30000/v1/chat/completions`) and model name (`localqwen`) are hardcoded in `src/ApiClient.cpp`. You can modify them there and rebuild the project. Future versions might include GUI or configuration file options.
*   **API Key:** Set the environment variable `OPENAI_API_KEY`. You can do this globally (e.g., in `~/.profile` or `~/.bashrc`) or when launching the application.
    ```bash
    export OPENAI_API_KEY="YOUR_API_KEY_OR_DUMMY_VALUE"
    # Launch the application (if it doesn't start automatically)
    # /usr/bin/knowbridge &
    ```
    *Note:* The application runs as a background process with no window. Restart your session or ensure the environment variable is available to system services/autostart.
*   **Global Shortcut:**
    1.  Open KDE "System Settings".
    2.  Go to "Shortcuts".
    3.  Find the application "Knowbridge" or the action "Modify Text (AI)".
    4.  Assign your desired key combination (default is `Ctrl+Alt+Space`).

## ▶️ Usage

1.  Ensure the `knowbridge` application is running (an icon should appear in the system tray) and the `OPENAI_API_KEY` environment variable is set.
2.  Open the application where you want to modify text (e.g., text editor, browser, terminal).
3.  Select the text you wish to modify. If nothing is selected, the utility might attempt to use the entire text of the element (if available via AT-SPI).
4.  Press the assigned global key combination (e.g., `Ctrl+Alt+Space`).
5.  A menu with action options (Fix Grammar, Improve Style, etc.) will appear near the cursor.
6.  Choose the desired action.
7.  A "Processing..." notification will appear.
8.  Wait for the result:
    *   **Successful in-place replacement:** The text in the application will be replaced, and a "Success" notification will appear.
    *   **Copy to clipboard:** A "Result Copied" notification will appear, and you can paste the result (`Ctrl+V`) from the clipboard. This happens if in-place replacement is not possible or fails.
    *   **Error:** An error notification will appear (e.g., API or network issue).

## 🚧 TODO

This is an early version. Here are some planned features and improvements:

*   **Codebase Refinement:** General improvements to structure and quality.
*   **Configuration Window:** Add a comprehensive GUI for settings (API endpoint, model, key management).
*   **Enhanced System Tray Menu:** Add options like accessing settings, viewing logs, or exiting.
*   **Custom Actions & Prompts:** Allow users to define custom actions with editable LLM prompts.
*   **Progress Indicator:** Provide visual feedback during processing (e.g., tray icon, notification).

## 🤝 Contributing

Contributions are welcome! Please open Issues for bug reports and feature requests. For code contributions, please open Pull Requests.
