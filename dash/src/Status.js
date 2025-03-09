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

    websocket.onopen = () => {
      message.success('Connection opened');
    };

    websocket.onclose = () => {
      message.error('Connection closed');
    };

    websocket.onerror = (err) => {
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

    return () => websocket.close();
  }, []);

  return (
    <>
      <Row>
        <Col span={3}>Telemetry</Col>
        <Col span={1}>{renderStatus(telemetryState)}</Col>
      </Row>
      <Row>
        <Col span={3}>GNSS</Col>
        <Col span={1}>{renderStatus(gnssState)}</Col>
      </Row>
    </>
  );
};

export default Status;
