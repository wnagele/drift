import React, { useState } from 'react';
import { Layout, Menu } from 'antd';
import { InfoCircleOutlined, EditOutlined } from '@ant-design/icons';
import Status from './Status';
import Config from './Config';

const { Content, Sider } = Layout;

const items = [
  {
    key: "status",
    label: "Status",
    icon: React.createElement(InfoCircleOutlined),
  },
  {
    key: "config",
    label: "Config",
    icon: React.createElement(EditOutlined),
  }
]

const App = () => {
  const [selectedKey, setSelectedKey] = useState("status");

  const renderContent = () => {
    switch (selectedKey) {
      case "status":
        return <Status />;
      case "config":
        return <Config />;
      default:
        return null;
    }
  };

  return (
    <Layout>
      <Sider breakpoint="md">
        <div style={{
            display: "flex",
            alignItems: "center",
            justifyContent: "center",
            fontSize: "20px",
            fontWeight: "bold",
            color: "white",
            textAlign: "center",
            padding: "10px"
          }}>
          DRIFT
        </div>
        <Menu theme="dark"
              items={items}
              defaultSelectedKeys={ [ "status" ] }
              onClick={(e) => setSelectedKey(e.key)} />
      </Sider>
      <Content>
        {renderContent()}
      </Content>
    </Layout>
  );
};

export default App;
