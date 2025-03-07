import React, { useEffect, useState } from 'react';
import { Form, Input, Button, message } from 'antd';
import axios from 'axios';

const API_ENDPOINT = '/api/config';

const Config = () => {
  const [form] = Form.useForm();
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    const getCurrentValues = async () => {
      try {
        setLoading(true);
        const { data } = await axios.get(API_ENDPOINT);
        form.setFieldsValue({
          "wifi_ssid": data["wifi"]["ssid"],
          "wifi_password": data["wifi"]["password"],
          "dri_ua_id":  data["dri"]["ua_id"],
          "dri_ua_desc":  data["dri"]["ua_desc"],
          "dri_op_id":  data["dri"]["op_id"]
        });
      } catch (error) {
        message.error("Could not get current values.");
      } finally {
        setLoading(false);
      }
    };
    getCurrentValues();
  }, [form]);

  const onFinish = async (data) => {
    try {
      setLoading(true);
      await axios.post(API_ENDPOINT, {
        "wifi": {
          "ssid": data["wifi_ssid"],
          "password": data["wifi_password"]
        },
        "dri": {
          "ua_id": data["dri_ua_id"],
          "ua_desc": data["dri_ua_desc"],
          "op_id": data["dri_op_id"]
        }
      });
      message.success("Config saved.");
    } catch (error) {
      message.error("Config failed to save.");
    } finally {
      setLoading(false);
    }
  };

  return (
    <Form
      form={form}
      layout="vertical"
      onFinish={onFinish}
    >
      <Form.Item
        label="WiFi SSID"
        name="wifi_ssid"
        rules={[
          { required: true, message: "WiFi SSID must be set." },
        ]}>
        <Input maxLength="32" showCount="true" />
      </Form.Item>
      
      <Form.Item
        label="WiFi Password"
        name="wifi_password"
        rules={[
          { required: false },
          { min: 8, message: "WiFi Password must be at least 8 characters long." },
        ]}
        >
        <Input.Password maxLength="64" showCount="true" />
      </Form.Item>
      
      <Form.Item
        label="Unmanned Aircraft ID"
        name="dri_ua_id"
        rules={[
          { required: true, message: "Unmanned Aircraft ID must be set." },
        ]}
      >
        <Input maxLength="20" showCount="true" />
      </Form.Item>
      
      <Form.Item
        label="Unmanned Aircraft Description"
        name="dri_ua_desc"
        rules={[
          { required: false },
        ]}
      >
        <Input maxLength="23" showCount="true" />
      </Form.Item>
      
      <Form.Item
        label="Operator ID"
        name="dri_op_id"
        rules={[
          { required: true, message: "Operator ID must be set." },
        ]}
      >
        <Input maxLength="20" showCount="true" />
      </Form.Item>

      <Form.Item>
        <Button type="primary" htmlType="submit" loading={loading}>Save</Button>
      </Form.Item>
    </Form>
  );
};

export default Config;
