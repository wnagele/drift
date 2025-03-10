import React, { useState, useEffect } from 'react';
import { Col, Row, message } from 'antd';
import { CheckCircleTwoTone, CloseCircleTwoTone, QuestionCircleTwoTone } from '@ant-design/icons';

const Status = () => {
  const renderStatus = (state) => {
    switch (state) {
      case true:
        return <CheckCircleTwoTone twoToneColor='#49e33b' />;
      case false:
        return <CloseCircleTwoTone twoToneColor='#e33b3b' />;
      default:
        return <QuestionCircleTwoTone twoToneColor='#e36b3b' />;
    }
  };

  const [telemetryState, setTelemetryState] = useState(null);
  const [gnssState, setGnssState] = useState(null);

  useEffect(() => {
    const url = 'ws://' + window.location.host + '/ws';
    const websocket = new WebSocket(url);
    var shuttingDown = false;

    websocket.onerror = (err) => {
      if (shuttingDown)
        return;
      console.error('WebSocket error: ', err);
      message.error('Connection error occured');
    };

    websocket.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.type === 'status') {
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

  return (
    <>
      <Row gutter={16}>
        <Col className="gutter-row" span={6}>Telemetry</Col>
        <Col className="gutter-row" span={6}>{renderStatus(telemetryState)}</Col>
      </Row>
      <Row  gutter={16}>
        <Col className="gutter-row" span={6}>GNSS</Col>
        <Col className="gutter-row" span={6}>{renderStatus(gnssState)}</Col>
      </Row>
    </>
  );
};

export default Status;
