import React from 'react';

// The one health-box presentation in the dash: a tinted, bordered box whose
// colour carries a three-way state (ok / degraded / down), with a leading dot,
// a label line and an optional detail line explaining the colour. Shared by
// every indicator in the sidebar's health block so they read as one kind of
// object rather than a pile of unrelated widgets.
//
// Styled for the dark sider, which is where every one of them lives. If a box
// is ever wanted on a light background, that is the point to reintroduce a
// variant — there was one here while the flags sat in the Status view, and it
// went away with them rather than being kept warm for a hypothetical caller.
//
// On a collapsed sider there is no room for the label, so the box shrinks to
// its `icon` plus the state dot, and the text moves into a native tooltip.
// The icon is what makes the collapsed block legible: three bare dots are all
// colour and no identity, so you could see that something was wrong without
// being able to tell what. The dot stays alongside rather than being folded
// into the icon's own colour, because a tinted glyph is far harder to read at
// 14px than a solid dot, and colour-blind users get two shapes to compare
// instead of one hue.
const StatusBox = ({ state, label, detail, icon, collapsed = false }) => {
  if (collapsed) {
    const tooltip = detail ? label + ' · ' + detail : label;
    return (
      <div className="status-box-collapsed" title={tooltip}>
        {icon}
        <span className={'status-dot status-dot-' + state} />
      </div>
    );
  }

  return (
    <div className={'status-box status-box-' + state}>
      <div className="status-box-label">
        <span className={'status-dot status-dot-' + state} />
        {label}
      </div>
      {detail && <div className="status-box-detail">{detail}</div>}
    </div>
  );
};

export default StatusBox;
