/* Copyright (C) 2017 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#ifndef ART_OPENJDKJVMTI_TI_CLASS_DEFINITION_H_
#define ART_OPENJDKJVMTI_TI_CLASS_DEFINITION_H_

#include <stddef.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "art_jvmti.h"

#include "base/array_ref.h"
#include "base/mem_map.h"
#include "events.h"

namespace openjdkjvmti {

// A struct that stores data needed for redefining/transforming classes. This structure should only
// even be accessed from a single thread and must not survive past the completion of the
// redefinition/retransformation function that created it.
class ArtClassDefinition {
 public:
  ArtClassDefinition()
      : klass_(nullptr),
        loader_(nullptr),
        name_(),
        protection_domain_(nullptr),
        dex_data_memory_(),
        dex_data_(),
        current_dex_memory_(),
        current_dex_file_(),
        redefined_(false),
        initialized_(false),
        structural_transform_update_(false) {}

  jvmtiError InitFirstLoad(const char* descriptor,
                           art::Handle<art::mirror::ClassLoader> klass_loader,
                           const art::DexFile& dex_file);
  jvmtiError Init(art::Thread* self, jclass klass);
  jvmtiError Init(art::Thread* self, const jvmtiClassDefinition& def);

  ArtClassDefinition(ArtClassDefinition&& o) = default;
  ArtClassDefinition& operator=(ArtClassDefinition&& o) = default;

  void SetNewDexData(jint new_dex_len, unsigned char* new_dex_data, ArtJvmtiEvent event) {
    DCHECK(IsInitialized());
    if (new_dex_data == nullptr) {
      return;
    } else {
      art::ArrayRef<const unsigned char> new_data(new_dex_data, new_dex_len);
      if (new_data != dex_data_) {
        dex_data_memory_.resize(new_dex_len);
        memcpy(dex_data_memory_.data(), new_dex_data, new_dex_len);
        dex_data_ = art::ArrayRef<const unsigned char>(dex_data_memory_);
        if (event == ArtJvmtiEvent::kStructuralDexFileLoadHook) {
          structural_transform_update_ = true;
        }
      }
    }
  }

  bool HasStructuralChanges() const {
    return structural_transform_update_;
  }

  art::ArrayRef<const unsigned char> GetNewOriginalDexFile() const {
    DCHECK(IsInitialized());
    if (redefined_) {
      return current_dex_file_;
    } else {
      return art::ArrayRef<const unsigned char>();
    }
  }

  bool IsModified() const REQUIRES_SHARED(art::Locks::mutator_lock_);

  bool IsInitialized() const {
    return initialized_;
  }

  jclass GetClass() const {
    DCHECK(IsInitialized());
    return klass_;
  }

  jobject GetLoader() const {
    DCHECK(IsInitialized());
    return loader_;
  }

  const std::string& GetName() const {
    DCHECK(IsInitialized());
    return name_;
  }

  jobject GetProtectionDomain() const {
    DCHECK(IsInitialized());
    return protection_domain_;
  }

  art::ArrayRef<const unsigned char> GetDexData() const {
    DCHECK(IsInitialized());
    return dex_data_;
  }

 private:
  jvmtiError InitCommon(art::Thread* self, jclass klass);
  jvmtiError Init(const art::DexFile& dex_file);

  jclass klass_;
  jobject loader_;
  std::string name_;
  jobject protection_domain_;

  // A unique_ptr to the current dex_data if it needs to be cleaned up.
  std::vector<unsigned char> dex_data_memory_;

  // A ref to the current dex data. This is either dex_data_memory_, or current_dex_file_. This is
  // what the dex file will be turned into.
  art::ArrayRef<const unsigned char> dex_data_;

  // This is only used if we failed to create a mmap to store the dequickened data
  std::vector<unsigned char> current_dex_memory_;

  // This is a dequickened version of what is loaded right now. It is either current_dex_memory_ (if
  // no other redefinition has ever happened to this) or the current dex_file_ directly (if this
  // class has been redefined, thus it cannot have any quickened stuff).
  art::ArrayRef<const unsigned char> current_dex_file_;

  bool redefined_;

  bool initialized_;

  // Set if we had a new dex from the given transform type.
  bool structural_transform_update_;

  DISALLOW_COPY_AND_ASSIGN(ArtClassDefinition);
};

}  // namespace openjdkjvmti

#endif  // ART_OPENJDKJVMTI_TI_CLASS_DEFINITION_H_
