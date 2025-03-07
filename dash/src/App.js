import React from 'react';
import { Layout, Menu } from 'antd';
import { InfoCircleOutlined, EditOutlined } from '@ant-design/icons';

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
  return (
    <Layout>
      <Sider>
        <Menu theme="dark" items={items} defaultSelectedKeys={ [ "status" ] }/>
      </Sider>
      <Content>
        TBD
      </Content>
    </Layout>
  );
};

export default App;
