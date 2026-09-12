import React, { useEffect, useState } from 'react';
import { Form, Input, Select, Switch, Button, Typography, message } from 'antd';
import axios from 'axios';

import {
  CHECKSUM_FAILED,
  CHECKSUM_PASSED,
  CHECKSUM_SKIPPED,
  joinOperatorInput,
  splitOperatorInput,
  validateOperatorInput,
} from './registration.js';

const API_ENDPOINT = '/api/config';

// "Other — no regional rules" posts an empty region. Named here so the
// intent is legible at the places that branch on it.
const REGION_NONE = '';

// The regions the firmware will accept (its allow-list is in config.cpp).
// The firmware is otherwise blind to the value: it stores it and serves it
// back, and every region *rule* - which identity fields are required, the
// EU/UK registration-number format - belongs here in the dash. See the
// region note in AGENTS.md.
const REGIONS = [
  { value: 'US', label: 'United States' },
  { value: 'EU', label: 'European Union' },
  { value: 'UK', label: 'United Kingdom' },
  // The honest answer for everywhere DRIFT does not model a registry -
  // including Australia and Canada, which have no mandate in force at all -
  // and the escape hatch if the EU/UK syntax check ever rejects a number it
  // should not. Posts "", which is also what an unconfigured device reports,
  // so the two are indistinguishable once saved.
  { value: REGION_NONE, label: 'Other — no regional rules' },
];

// Each authority calls the operator's registration by a different name, and
// 14 CFR 89.315 has no such field at all - a US broadcast module carries a
// serial number and nothing else - so the field is hidden under US. Every
// label here is the issuing authority's own term.
const OPERATOR_LABELS = {
  EU: 'Operator Registration Number',
  UK: 'Remote ID Number',
  // No registry is being modelled, so no authority's term applies.
  [REGION_NONE]: 'Operator ID',
};

// Under "Other" the operator ID is an opaque string capped at ODID_ID_SIZE,
// exactly as it was before regions existed: no registry means no 16+3 split
// to perform, and splitting anyway would silently move the tail of a plain
// 20-character ID into the verification code.
const OPERATOR_ID_MAX = 20;

// The registration number is 16 characters and the verification code 3, but
// the input accepts the whole pasted string plus separators in either
// official rendering, so the cap is generous and sanitizing does the work.
const OPERATOR_INPUT_MAX = 32;

// The two verification indicators shown under the operator field.
//
// The syntax check is objective and blocking, so it only ever reads pass or
// fail. The security check has three states, and the third matters: the
// Luhn mod-36 checksum can only be computed with the 3 secret digits, and
// most national registries appear not to issue them at all (Austria issues
// only the 16-character number). Rendering "no code supplied" as a failure
// would show the majority of perfectly compliant operators a red cross, so
// it reads as a neutral "not available" instead.
const OperatorChecks = ({ value, region }) => {
  if (!value) return null;
  const { secret, structure, checksum } = validateOperatorInput(value, region);

  const CHECKSUM_TEXT = {
    [CHECKSUM_PASSED]: 'Security check passed',
    [CHECKSUM_FAILED]: `Security check failed${checksum.error ? ` - ${checksum.error}` : ''}`,
    [CHECKSUM_SKIPPED]: 'Security check not available - no verification code supplied',
  };
  const CHECKSUM_TYPE = {
    [CHECKSUM_PASSED]: 'success',
    [CHECKSUM_FAILED]: 'danger',
    [CHECKSUM_SKIPPED]: 'secondary',
  };

  return (
    <span>
      <Typography.Text type={structure.valid ? 'success' : 'danger'}>
        {structure.valid ? 'Syntax check passed' : `Syntax check failed - ${structure.error}`}
      </Typography.Text>
      <br />
      <Typography.Text type={CHECKSUM_TYPE[checksum.status]}>
        {CHECKSUM_TEXT[checksum.status]}
      </Typography.Text>
      {secret && (
        <>
          <br />
          <Typography.Text type="secondary">
            The verification code is stored but never broadcast.
          </Typography.Text>
        </>
      )}
    </span>
  );
};

// How the single operator input maps onto the two stored fields, per region.
// Three distinct behaviours, and the middle one is the subtle case: under
// "Other" no registry is in play, so there are no secret digits to split off
// and the ID is stored verbatim. Splitting anyway would silently move the
// tail of a plain 20-character ID into the verification code.
function operatorFor(region, value) {
  // 14 CFR 89.315 defines no operator-registration field, so there is
  // nothing to carry; an empty op_id also leaves OperatorID invalid in the
  // firmware, and therefore unbroadcast.
  if (region === 'US') return { number: '', secret: '' };
  if (region === REGION_NONE) return { number: (value || '').trim(), secret: '' };
  return splitOperatorInput(value);
}

const Config = () => {
  const [form] = Form.useForm();
  const [loading, setLoading] = useState(false);
  // The operator field's label, visibility and validation all key off the
  // selected region, so the form has to re-render when it changes.
  const region = Form.useWatch('dri_region', form);
  const operatorInput = Form.useWatch('dri_op_id', form);
  // "Other": the operator ID is opaque, so no label from an authority, no
  // format rules and no verification indicators apply.
  const regionless = region === REGION_NONE;

  useEffect(() => {
    const getCurrentValues = async () => {
      try {
        setLoading(true);
        const { data } = await axios.get(API_ENDPOINT);
        form.setFieldsValue({
          "wifi_ssid": data["wifi"]["ssid"],
          "wifi_password": data["wifi"]["password"],
          // "" is a selectable choice ("Other"), not a missing value, so it
          // is passed through as-is. The consequence is deliberate but worth
          // knowing: a factory-fresh device also reports "", so it loads
          // showing "Other" rather than prompting for a region.
          "dri_region": data["dri"]["region"],
          "dri_ua_id":  data["dri"]["ua_id"],
          "dri_ua_desc":  data["dri"]["ua_desc"],
          // The two stored halves are rejoined into the one field the user
          // sees, in EASA's own rendering. They are stored apart so only the
          // public half can reach the wire. Under "Other" there is no
          // verification code to rejoin, so the ID shows verbatim.
          "dri_op_id":  joinOperatorInput(data["dri"]["op_id"], data["dri"]["op_secret"]),
          "dri_bt5_enabled": data["dri"]["bt5_enabled"] ?? true,
          "dri_wifi_beacon_enabled": data["dri"]["wifi_beacon_enabled"] ?? true,
          "dri_wifi_nan_enabled": data["dri"]["wifi_nan_enabled"] ?? false
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
    const operator = operatorFor(data["dri_region"], data["dri_op_id"]);
    try {
      setLoading(true);
      await axios.post(API_ENDPOINT, {
        "wifi": {
          "ssid": data["wifi_ssid"],
          "password": data["wifi_password"]
        },
        "dri": {
          // Required by the device, which takes a complete document and
          // rejects an unknown region. The form's required rule means an
          // unset region never gets this far.
          "region": data["dri_region"],
          "ua_id": data["dri_ua_id"],
          "ua_desc": data["dri_ua_desc"],
          "op_id": operator.number,
          "op_secret": operator.secret,
          "bt5_enabled": data["dri_bt5_enabled"] ?? true,
          "wifi_beacon_enabled": data["dri_wifi_beacon_enabled"] ?? true,
          "wifi_nan_enabled": data["dri_wifi_nan_enabled"] ?? false
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
      
      {/* Region gates the identity fields below it, so it heads the DRI
          group. Required, and with no default: DRIFT cannot guess the
          jurisdiction and guessing wrong is a compliance problem, so the
          user has to say. */}
      <Form.Item
        label="Region"
        name="dri_region"
        rules={[
          // Not `required`: antd treats "" as empty, and "" is the valid
          // "Other" choice. Only a genuinely unset value (a failed GET, so
          // setFieldsValue never ran) is an error.
          {
            validator: (_, value) =>
              value === undefined
                ? Promise.reject(new Error("Region must be set."))
                : Promise.resolve(),
          },
        ]}
      >
        {/* virtual={false} because three options have nothing to virtualize,
            and rc-virtual-list renders only part of the list when it cannot
            measure heights - which is every non-browser environment. */}
        <Select
          placeholder="Select the region you operate in"
          options={REGIONS}
          virtual={false}
        />
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
      
      {/* Hidden entirely under US, which defines no operator-registration
          field. onFinish clears both halves there, so nothing stale is
          submitted. Under "Other" the field is shown but unvalidated and
          optional - the user enters whatever their jurisdiction expects. */}
      {region !== 'US' && (
        <Form.Item
          label={OPERATOR_LABELS[region] || OPERATOR_LABELS.EU}
          name="dri_op_id"
          extra={regionless ? undefined : <OperatorChecks value={operatorInput} region={region} />}
          rules={regionless ? [] : [
            { required: true, message: `${OPERATOR_LABELS[region] || OPERATOR_LABELS.EU} must be set.` },
            // Blocking: a structurally wrong number cannot be saved. The
            // allow-list covers all 31 EASA member states precisely because
            // this blocks - a missing country code would make the device
            // unconfigurable for that operator. "Other" is the escape hatch
            // if it ever rejects a number it should not.
            {
              validator: (_, value) => {
                const { structure } = validateOperatorInput(value || '', region);
                return structure.valid ? Promise.resolve() : Promise.reject(new Error(structure.error));
              },
            },
          ]}
        >
          <Input
            maxLength={regionless ? OPERATOR_ID_MAX : OPERATOR_INPUT_MAX}
            showCount={regionless ? true : undefined}
            placeholder={regionless ? undefined
              : (region === 'UK' ? 'GBR gc284pmztcrt l 2ot' : 'FIN87astrdge12k8-xyz')}
          />
        </Form.Item>
      )}

      <Form.Item
        label="Bluetooth 5 Long Range"
        name="dri_bt5_enabled"
        valuePropName="checked"
      >
        <Switch />
      </Form.Item>

      <Form.Item
        label="Wi-Fi Beacon"
        name="dri_wifi_beacon_enabled"
        valuePropName="checked"
      >
        <Switch />
      </Form.Item>

      <Form.Item
        label="Wi-Fi NAN"
        name="dri_wifi_nan_enabled"
        valuePropName="checked"
      >
        <Switch />
      </Form.Item>

      <Form.Item>
        <Button type="primary" htmlType="submit" loading={loading}>Save</Button>
      </Form.Item>
    </Form>
  );
};

export default Config;
