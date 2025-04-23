[Desktop Entry]
Name=Knowbridge
Comment=Modify selected text using OpenAI via global shortcut
# Make sure the executable name matches your CMakeLists.txt target
Exec=@CMAKE_INSTALL_PREFIX@/@KDE_INSTALL_BINDIR@/knowbridge
Icon=accessories-text-editor # Replace with your actual icon name if you provide one
Type=Application
Terminal=false
Categories=Qt;KDE;Utility;TextTools;
X-KDE-ServiceTypes=KCModule,Application # Allows configuration via System Settings->Shortcuts
X-KDE-System-Settings-Parent-Category=shortcuts
X-KDE-Keywords=text,ai,openai,modify,grammar,style,shortcut