// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_eranames.h"

#include "fxjs/xfa/cjx_node.h"
#include "xfa/fxfa/parser/cxfa_document.h"

namespace {

const CXFA_Node::PropertyData kEraNamesPropertyData[] = {
    {XFA_Element::Era, 2, {}},
};

}  // namespace

CXFA_EraNames::CXFA_EraNames(CXFA_Document* doc, XFA_PacketType packet)
    : CXFA_Node(doc,
                packet,
                XFA_XDPPACKET_LocaleSet,
                XFA_ObjectType::Node,
                XFA_Element::EraNames,
                kEraNamesPropertyData,
                {},
                cppgc::MakeGarbageCollected<CJX_Node>(
                    doc->GetHeap()->GetAllocationHandle(),
                    this)) {}

CXFA_EraNames::~CXFA_EraNames() = default;
