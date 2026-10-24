// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// Implementation notes; declarations live in TurboMCPGraphStructure.h.
//
// Copy duplicates existing nodes with their allocated pin trees. Do not pass an
// allocated source node to SpawnNodeFromTemplate: that helper allocates pins again.
// Clear duplicated link references without breaking source links, then reconstruct
// the planned connections between destination nodes. See TurboMCPCopyFailureTest.cpp.
//
// Copy does not compile or save. Eligibility guards reject unsupported structural
// creation. Undo completion alone does not prove full restoration, and node-class
// paste hooks and cross-Blueprint references still require broader acceptance.
//
// Collapse currently analyzes selection boundaries but does not extract a function
// or macro. Public engine APIs existing is not evidence that this feature works.
//
// Evidence scope matters: copying between two graphs in one Blueprint does not
// establish cross-Blueprint compatibility. Tests must state which case they cover.
