import React, { useEffect, useState } from 'react';
import { Layout, Menu, message } from 'antd';
import { EditOutlined, InfoCircleOutlined } from '@ant-design/icons';
import Status from './Status';
import SidebarHealth from './SidebarHealth';
import useStatusSocket from './useStatusSocket';
import Config from './Config';
import axios from 'axios';

const API_ENDPOINT = '/debug/info';

const { Content, Sider, Footer } = Layout;

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
  const [buildInfo, setBuildInfo] = useState("LOADING");
  const [collapsed, setCollapsed] = useState(false);
  // One /ws connection for the whole app: the sidebar health block is
  // visible from every tab, and the Status view reads the transmit rates
  // off the same message.
  const status = useStatusSocket();

  useEffect(() => {
    const getBuildInfo = async () => {
      try {
        const { data } = await axios.get(API_ENDPOINT);
        if (data["version"] !== null) {
          setBuildInfo(data["version"]);
        } else if (data["git_ref"] !== null) {
          setBuildInfo(data["git_ref"].substring(0, 7));
        } else if (data["build_time"] !== null) {
          setBuildInfo(data["build_time"]);
        } else {
          setBuildInfo("UNKNOWN");
        }
      } catch (error) {
        message.error("Could not get build info.");
      }
    };
    getBuildInfo();
  }, []);

  const renderContent = () => {
    switch (selectedKey) {
      case "status":
        return <Status txState={status.txState}
                       broadcastState={status.broadcastState}
                       broadcastAgeMs={status.broadcastAgeMs} />;
      case "config":
        return <Config />;
      default:
        return null;
    }
  };

  return (
    <Layout>
      <Sider breakpoint="md" onCollapse={setCollapsed}>
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
        <SidebarHealth collapsed={collapsed} status={status} />
        <Menu theme="dark"
              items={items}
              defaultSelectedKeys={ [ "status" ] }
              onClick={(e) => setSelectedKey(e.key)} />
      </Sider>
      <Layout>
        <Content>
          {renderContent()}
        </Content>
        <Footer>Build Info: {buildInfo}</Footer>
      </Layout>
    </Layout>
  );
};

export default App;
