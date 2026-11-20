const status = document.getElementById("status");
const installer = document.querySelector("esp-web-install-button");

if (!window.isSecureContext) {
  status.textContent = "Open the installer over HTTPS or localhost to use USB.";
} else if (!("serial" in navigator)) {
  status.textContent = "For USB installation, open this page in desktop Chrome or Edge.";
} else {
  try {
    await import("https://unpkg.com/esp-web-tools@10.1.1/dist/web/install-button.js?module");
    await customElements.whenDefined("esp-web-install-button");
    installer.hidden = false;
    status.hidden = true;
  } catch {
    status.textContent = "The USB installer could not load. Reload this page or use the BIN with esptool at address 0x0.";
  }
}
