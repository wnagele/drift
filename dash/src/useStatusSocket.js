import { useState, useEffect, useRef } from 'react';
import { message } from 'antd';

// The firmware pushes a status message once per second (taskSendStatus in
// main.cpp). An open socket that stays silent longer than this is wedged or
// the link is half-dead — surfaced as "stale" instead of "connected",
// because a plain readyState check would keep looking healthy while the
// status view quietly shows frozen data.
export const STALE_AFTER_MS = 5000;
const TICK_MS = 1000;

// Owns the /ws connection and derives its health. Called once in App so the
// sidebar connection box and the Status view share a single socket that
// stays connected across tab switches.
//
// Returns:
//   connection      'connecting' | 'connected' | 'closed'
//   stale           no status message yet, or none within STALE_AFTER_MS
//   msgAgeMs        ms since the last status message, or null if none yet
//   closedAgeMs     ms since the connection was lost, or null unless closed
//   telemetryState  true | false | null (no status message yet)
//   gnssState       true | false | null
const useStatusSocket = () => {
  const [telemetryState, setTelemetryState] = useState(null);
  const [gnssState, setGnssState] = useState(null);
  const [connection, setConnection] = useState('connecting');
  const [now, setNow] = useState(Date.now());
  const lastMessageAt = useRef(null);
  const closedAt = useRef(null);

  useEffect(() => {
    const url = 'ws://' + window.location.host + '/ws';
    const websocket = new WebSocket(url);
    var shuttingDown = false;

    websocket.onopen = () => {
      if (!shuttingDown)
        setConnection('connected');
    };

    websocket.onclose = () => {
      if (!shuttingDown) {
        closedAt.current = Date.now();
        setNow(Date.now());
        setConnection('closed');
      }
    };

    websocket.onerror = (err) => {
      if (shuttingDown)
        return;
      console.error('WebSocket error: ', err);
      closedAt.current = Date.now();
      setNow(Date.now());
      setConnection('closed');
      message.error('Connection error occured');
    };

    websocket.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.type === 'status') {
          lastMessageAt.current = Date.now();
          setTelemetryState(data.telemetry);
          setGnssState(data.gnss);
        }
      } catch (err) {
        console.error('Invalid message: ', event.data);
      }
    };

    return () => {
      shuttingDown = true;
      websocket.close()
    };
  }, []);

  // Liveness tick: re-render once a second so consumers can report how long
  // ago the last status message arrived and flip to stale when the socket
  // goes silent.
  useEffect(() => {
    const tick = setInterval(() => setNow(Date.now()), TICK_MS);
    return () => clearInterval(tick);
  }, []);

  // A socket is only healthy once a status message has arrived, and stays
  // healthy while messages keep arriving within STALE_AFTER_MS. Before the
  // first message the state is correctly "no data" — it clears as soon as
  // the 1 Hz push lands, so there is deliberately no grace period from the
  // open event. Ages are clamped at 0: `now` is sampled from the 1 s tick
  // and can lag a just-arrived message or close event.
  const msgAgeMs = lastMessageAt.current === null
    ? null
    : Math.max(0, now - lastMessageAt.current);
  const stale = connection === 'connected' &&
                (msgAgeMs === null || msgAgeMs >= STALE_AFTER_MS);
  const closedAgeMs = connection === 'closed' && closedAt.current !== null
    ? Math.max(0, now - closedAt.current)
    : null;

  return { connection, stale, msgAgeMs, closedAgeMs, telemetryState, gnssState };
};

export default useStatusSocket;
