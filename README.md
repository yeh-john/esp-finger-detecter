# Device Connection & Troubleshooting Guide

## ⚠️ Important
When connecting the system, always follow this order:

1. Start the **Display Device** first.
2. Wait until the display device has **booted and is ready for connection**.
3. After that, start the **Finger Detector**.

If the Finger Detector starts before the Display Device is ready, the connection may fail.

---

## How the Device is Detected

The system attempts to detect the device using the following order:

1. First, it tries to connect using the **SSID (Device Name)**.
2. If the SSID connection fails, it switches to using the **MAC Address**.

---

## Known Problems

### 1. Motor Does Not Spin Properly
**Issue:**
- The motor only spins a little bit and then stops.

**Status:**
- Cause still under investigation.

---

### 2. Finger Detector Power Issue
**Issue:**
- When the Finger Detector is powered **only by battery**, it does **not connect to the display device**.
- When powered using **USB Type-C**, the connection works normally.

**Possible Cause:**
- Battery may not be supplying stable voltage.
- The wireless module may require more power during startup.

---

## Notes
- Always verify the **Display Device is fully booted** before starting the Finger Detector.
- If connection fails, try reconnecting using the **MAC address** instead of the SSID.
