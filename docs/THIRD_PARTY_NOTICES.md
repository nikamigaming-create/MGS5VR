# Third-party notices

The authored MGS5VR implementation is distributed under the root `LICENSE`
(MIT). Third-party components retain their own licenses and copyrights.
Complete license texts are retained in `licenses/` in both source and install
trees. No game binaries, assets or private reverse-engineering dumps are included.

| Component | Pinned source revision | License / notice |
| --- | --- | --- |
| [OpenXR SDK](https://github.com/KhronosGroup/OpenXR-SDK) | `977f6675bc0057d5a54ed290cb5c71c699b1c0ab` (1.1.49) | [Apache 2.0](../licenses/OpenXR.txt), component notices in `licenses/OpenXR/` |
| [JsonCpp](https://github.com/KhronosGroup/OpenXR-SDK/tree/977f6675bc0057d5a54ed290cb5c71c699b1c0ab/src/external/jsoncpp) | Included in the pinned OpenXR SDK | [MIT / public-domain dedication](../licenses/JsonCpp.txt) |
| [MinHook](https://github.com/TsudaKageyu/minhook) | `c3fcafdc10146beb5919319d0683e44e3c30d537` (1.3.4) | [BSD 2-Clause, including HDE notices](../licenses/MinHook.txt) |

The native variable-frame-rate and critical-worker scheduling adapter follows
[MGSVFix by Lyall](https://codeberg.org/Lyall/MGSVFix), with instruction sites
independently verified against the supported executable. The corresponding
[MIT notice](../licenses/MGSVFix.txt) is included. See [performance](PERFORMANCE.md)
for the adapted behavior; no MGSVFix binary is bundled or required.

The Windows SDK, Visual C++ runtime, installed OpenXR runtime and legally owned
game are external platform/runtime requirements. The optional simulator recorder
calls a separately installed Meta XR Operator; its binaries are not bundled and
it is not a build dependency of the mod. The mod does not redistribute a simulator.

The diagnostic camera signature was identified using the MGS5 component of
[Injectable Generic Camera System](https://github.com/FransBouma/InjectableGenericCameraSystem),
revision `42e76dfcf4d60f08b0828443335deb93434a9ea8`. Its published camera layout was
independently checked against the owned executable. The observer and assembly
preservation shim in this project are new diagnostic implementations.

Copyright (c) 2017, Frans Bouma. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
