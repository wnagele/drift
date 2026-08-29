import React from 'react';
import { Col, Row } from 'antd';
import { CheckCircleTwoTone, CloseCircleTwoTone, QuestionCircleTwoTone } from '@ant-design/icons';

// Status view: the telemetry/GNSS flags from the latest status message.
// Presentational — the /ws connection is owned by useStatusSocket in App
// and displayed in the sidebar ConnectionBox, so it stays visible from
// every tab; this view carries what the device is reporting.
const Status = ({ telemetryState, gnssState }) => {
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
