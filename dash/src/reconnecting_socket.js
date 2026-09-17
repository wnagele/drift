// A WebSocket that survives its own death. Wraps the browser's WebSocket
// in the reconnect policy the dash's /ws link needs — the flakiest leg of
// the dashboard: the device reboots after every config save, the SoftAP
// drops when the operator walks out of range, and mobile browsers kill
// background-tab sockets — so a closed socket is retried forever with
// capped exponential backoff (an attempt that fails without ever opening
// doubles the next delay).
//
// The event surface is the native one (onopen/onclose/onerror/onmessage
// plus close()), so consumers see a single long-lived socket: events from
// a socket replaced by a reconnect are suppressed rather than forwarded,
// and close() is a permanent shutdown that cancels all pending retries.
// What the events mean remains the consumer's business — this class owns
// when to connect, nothing else.

// Backoff starts fast — a momentary drop (the SoftAP hiccuping, the browser
// killing the socket on a brief tab switch) is masked within a few hundred
// milliseconds — then doubles per failed attempt up to a cap: the device
// takes ~10 s to come back from a config-save reboot, so an unreachable
// host must not push the delay beyond that before the link can be found
// again.
export const RETRY_BASE_MS = 250;
export const RETRY_MAX_MS = 10000;

// Browsers expose no WebSocket connect timeout, and an attempt to a host
// that is gone can hang in CONNECTING for tens of seconds — cutting each
// attempt short keeps the retry loop moving on a dead link.
export const CONNECT_TIMEOUT_MS = 5000;

class ReconnectingSocket {
  constructor(url) {
    this.url = url;
    this.onopen = null;
    this.onclose = null;
    this.onerror = null;
    this.onmessage = null;
    this.websocket = null;
    this.retryTimer = null;
    this.attemptTimer = null;
    this.attempts = 0;
    this.shutDown = false;
    this.onVisibility = () => {
      if (this.shutDown || document.visibilityState !== 'visible')
        return;
      // Reconnect at once when the tab comes back: the browser killed the
      // background socket and froze the pending retry timer, so waiting
      // for it would only delay recovery. A still-live socket is left
      // alone.
      if (this.websocket !== null && this.websocket.readyState >= WebSocket.CLOSING)
        this.reconnectNow();
    };
    document.addEventListener('visibilitychange', this.onVisibility);

    this.connect();
  }

  // Permanent shutdown: no more retries, no more events.
  close() {
    this.shutDown = true;
    document.removeEventListener('visibilitychange', this.onVisibility);
    this.clearRetryTimer();
    this.clearAttemptTimer();
    if (this.websocket !== null)
      this.websocket.close();
  }

  clearRetryTimer() {
    if (this.retryTimer !== null) {
      clearTimeout(this.retryTimer);
      this.retryTimer = null;
    }
  }

  clearAttemptTimer() {
    if (this.attemptTimer !== null) {
      clearTimeout(this.attemptTimer);
      this.attemptTimer = null;
    }
  }

  reconnectNow() {
    this.clearRetryTimer();
    this.clearAttemptTimer();
    this.attempts = 0;
    this.connect();
  }

  scheduleRetry() {
    if (this.shutDown || this.retryTimer !== null)
      return;
    const delay = Math.min(RETRY_BASE_MS * Math.pow(2, this.attempts), RETRY_MAX_MS);
    this.attempts += 1;
    this.retryTimer = setTimeout(() => {
      this.retryTimer = null;
      this.connect();
    }, delay);
  }

  connect() {
    const ws = new WebSocket(this.url);
    this.websocket = ws;

    // A socket replaced by a reconnect can still deliver its deferred
    // close event afterwards — a background tab killed by the browser does
    // exactly that on resume — and it must not clobber the new one.
    ws.onopen = () => {
      if (this.shutDown || this.websocket !== ws)
        return;
      this.clearAttemptTimer();
      this.attempts = 0;
      if (this.onopen !== null)
        this.onopen();
    };

    ws.onclose = () => {
      if (this.shutDown || this.websocket !== ws)
        return;
      this.clearAttemptTimer();
      if (this.onclose !== null)
        this.onclose();
      this.scheduleRetry();
    };

    ws.onerror = (err) => {
      if (this.shutDown || this.websocket !== ws)
        return;
      if (this.onerror !== null)
        this.onerror(err);
    };

    ws.onmessage = (event) => {
      if (this.shutDown || this.websocket !== ws)
        return;
      if (this.onmessage !== null)
        this.onmessage(event);
    };

    this.attemptTimer = setTimeout(() => {
      this.attemptTimer = null;
      ws.close();
    }, CONNECT_TIMEOUT_MS);
  }
}

export default ReconnectingSocket;
