---
layout: page
title: Regulations
has_children: true
nav_order: 3
permalink: /regulations/
---
# Regulations

A per-jurisdiction summary of our best current understanding of the Remote ID /
Drone Remote Identification (DRI) regulations that reference the ASTM F3411 /
ASD-STAN prEN 4709-002 broadcast message set — the standard DRIFT implements — and
what each one means at the technical level: which messages and fields are required,
what has to go out over which radio transport, and what identifiers a user needs to
obtain and enter into the device.

{: .important }
> This section is a research summary, not legal advice. Regulations change, and some
> of the technical detail below rests on standards text that is paywalled and could
> only be verified against public abstracts/scope pages (marked inline where that's
> the case). Verify against the primary sources linked on each page before relying on
> this for your own compliance. See the [Legal](/legal/) page.

## Jurisdictions

| Jurisdiction | Status | Wire-compatible with DRIFT's ASTM F3411 / ODID format |
|:---|:---|:---|
| [United States](/regulations/us/) | In force since 2023-09-16 | Yes |
| [European Union](/regulations/eu/) | In force | Yes |
| [United Kingdom](/regulations/uk/) | Partially in force; full retrofit mandate from 2028-01-01 | Yes |
| [Japan](/regulations/japan/) | In force since 2022 | Yes, with mandatory extensions (dual ID, authentication) |
| [Australia](/regulations/australia/) | No mandate yet — discussion-paper stage | Anticipated |
| [Canada](/regulations/canada/) | No mandate yet — proposed rule (NPA), targeting 2029/2030 | Yes, explicitly named |
| [China](/regulations/china/) | In force since 2026-05-01 | **No — separate, incompatible wire protocol** |

## Other jurisdictions

A brief scan turned up no ASTM F3411-compatible broadcast mandate worth a dedicated
page yet for South Korea, India, Brazil, or New Zealand. Singapore reportedly brought
a Broadcast Remote ID rule into force on 2025-12-01 per a secondary source (Transport
Canada's own NPA); this is unverified against a primary Singaporean source and worth
a follow-up look before assuming it's ASTM-F3411-based.

## Reading a region page

Each page follows the same structure:

- **Applicability** — who is actually covered (weight thresholds, product category,
  exemptions).
- **Timeline** — effective/enforcement dates, as best known today.
- **Required messages & fields** — the ASTM F3411 / ODID message set and which
  specific field on the wire carries which piece of information.
- **Transports** — which radio methods are mandated, allowed, or excluded, and any
  region-specific technical parameters.
- **Identity & registration fields** — what a user has to provide, which DRIFT
  config field and wire field it ends up in, and where to actually obtain it.
- **Not required** — items commonly assumed to matter that don't, for this
  jurisdiction and product category.
- **Sources** — every claim traced back to a citation.
