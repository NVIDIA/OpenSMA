--
--  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
--  All rights reserved.
--  SPDX-License-Identifier: Apache-2.0
--
--  Licensed under the Apache License, Version 2.0 (the "License");
--  you may not use this file except in compliance with the License.
--  You may obtain a copy of the License at
--
--  http://www.apache.org/licenses/LICENSE-2.0
--
--  Unless required by applicable law or agreed to in writing, software
--  distributed under the License is distributed on an "AS IS" BASIS,
--  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
--  See the License for the specific language governing permissions and
--  limitations under the License.
--
with System;
with RFLX.SPDM;
with RFLX.SPDM_Responder.Session;
with RFLX.SPDM_Responder.Digests_Data;
with RFLX.SPDM_Responder.Signature;
with RFLX.SPDM_Responder.Opaque_Data;
with RFLX.SPDM.Certificate_Response;
with RFLX.SPDM.Nonce;
with RFLX.SPDM.DMTF_Measurement_Field;
with RFLX.RFLX_Types;

--  @summary
--  Example platform implementation for SPDM.
--
--  @description
--  This package serves as both an example and a default C binding for
--  the platform code required by SPDM. It includes the minimum required
--  implementation that consists of a Context type derived from the SPDM
--  responder session context and implementations for all of its abstract
--  subprograms.
--  The implementations of these subprograms contain a binding for a C interface.
--  This binding is not required for pure SPARK/Ada projects and can be replaced.
--  Additionally it contains an initialization procedure that is used for the sole
--  purpose of initializing the example C implementation. It is not strictly required
--  as an implementation is free to decide how and when to initialize its custom
--  Context type.
package SPDM_C_Responder with
   SPARK_Mode,
   Elaborate_Body
is

   --  Implementation defined.
   --
   --  This record can be defined freely by the implementer to
   --  hold any value required by the platform. Memory management
   --  for this struct is also the responsibility of the
   --  implementer.
   type Context is new RFLX.SPDM_Responder.Session.Context with record
      Instance : System.Address := System.Null_Address;
   end record;

   --  Ensure initialization of Ctx.Instance.
   --
   --  This procedure is both implemented and called by the platform code.
   --  It is called before the start of the state machine and ensures that
   --  Context.Instance is initialized. It is not necessary for the state
   --  machine to function. If the platform code has other means of initializing
   --  the Context this procedure can be removed.
   --
   --  @param Ctx Context.
   procedure Plat_Initialize (Ctx : in out Context);

   --  Return CT exponent (DSP0274_1.1.0 [178]).
   --
   --  @param Ctx Context.
   --  @param Result CT exponent.
   overriding
   procedure Plat_Cfg_CT_Exponent
      (Ctx    : in out Context;
       Result :    out RFLX.SPDM.CT_Exponent);

   --  Indicate whether measurements without restart are supported (DSP0274_1.1.0 [178]).
   --
   --  @param Ctx Context.
   --  @param Result True if measurements without restart are supported.
   overriding
   procedure Plat_Cfg_Cap_Meas_Fresh
      (Ctx    : in out Context;
       Result :    out Boolean);

   --  Indicate which type of measurements are supported (DSP0274_1.1.0 [178]).
   --
   --  @param Ctx Context.
   --  @param Result Measurement capability.
   overriding
   procedure Plat_Cfg_Cap_Meas
      (Ctx    : in out Context;
       Result :    out RFLX.SPDM.Meas_Cap);

   --  Indicate whether challenge authentication is supported (DSP0274_1.1.0 [178]).
   --
   --  @param Ctx Context.
   --  @param Result True if challenge authentication is supported.
   overriding
   procedure Plat_Cfg_Cap_Chal
      (Ctx    : in out Context;
       Result :    out Boolean);

   --  Indicate whether digests and certificate responses are supported (DSP0274_1.1.0 [178]).
   --
   --  @param Ctx Context.
   --  @param Result True if digests and certificate responses are supported.
   overriding
   procedure Plat_Cfg_Cap_Cert
      (Ctx    : in out Context;
       Result :    out Boolean);

   --  Indicate whether responder is able to cache the negotiated state after reset (DSP0274_1.1.0 [178]).
   --  Indicate whether key update is supported (DSP0274_1.1.0 [178]).
   --
   --  @param Ctx Context.
   --  @param Result True if caching is supported.
   overriding
   procedure Plat_Cfg_Cap_Cache
      (Ctx    : in out Context;
       Result :    out Boolean);

   --  Indicate whether key update is supported (DSP0274_1.1.0 [178]).
   --
   --  @param Ctx Context.
   --  @param Result True if key update is supported.
   overriding
   procedure Plat_Cfg_Cap_Key_Upd
      (Ctx    : in out Context;
       Result :    out Boolean);

   --  Indicate whether heartbeat messages are supported (DSP0274_1.1.0 [178]).
   --
   --  @param Ctx Context.
   --  @param Result True if heartbeat messages are supported.
   overriding
   procedure Plat_Cfg_Cap_Hbeat
      (Ctx    : in out Context;
       Result :    out Boolean);

   --  Indicate whether encapsulated messages are supported (DSP0274_1.1.0 [178]).
   --
   --  @param Ctx Context.
   --  @param Result True if encapsulated messages are supported.
   overriding
   procedure Plat_Cfg_Cap_Encap
      (Ctx    : in out Context;
       Result :    out Boolean);

   --  Indicate whether mutual authentication is supported (DSP0274_1.1.0 [178]).
   --
   --  @param Ctx Context.
   --  @param Result True if mutual authentication is supported.
   overriding
   procedure Plat_Cfg_Cap_Mut_Auth
      (Ctx    : in out Context;
       Result :    out Boolean);

   --  Indicate whether the public key of the responder was provisioned to the requester (DSP0274_1.1.0 [178]).
   --
   --  @param Ctx Context.
   --  @param Result True if the public key was provisioned.
   overriding
   procedure Plat_Cfg_Cap_Pub_Key_ID
      (Ctx    : in out Context;
       Result :    out Boolean);

   --  Select measurement hash algorithm (DSP0274_1.1.0 [185]).
   --
   --  The arguments describe the hash algorithms supported by the requester.
   --  This function should select one of the provided algorithms.
   --
   --  @param Ctx Context.
   --  @param TPM_ALG_SHA_256 SHA-256 supported and requested.
   --  @param TPM_ALG_SHA_384 SHA-384 supported and requested.
   --  @param TPM_ALG_SHA_512 SHA-512 supported and requested.
   --  @param TPM_ALG_SHA3_256 SHA3-256 supported and requested.
   --  @param TPM_ALG_SHA3_384 SHA3-384 supported and requested.
   --  @param TPM_ALG_SHA3_512 SHA3-512 supported and requested.
   --  @param Raw_Bit_Streams_Only Raw bit streams supported and requested.
   --  @param Result Selected algorithm.
   overriding
   procedure Plat_Cfg_Sel_Measurement_Hash_Algo
      (Ctx                  : in out Context;
       TPM_ALG_SHA_256      :        Boolean;
       TPM_ALG_SHA_384      :        Boolean;
       TPM_ALG_SHA_512      :        Boolean;
       TPM_ALG_SHA3_256     :        Boolean;
       TPM_ALG_SHA3_384     :        Boolean;
       TPM_ALG_SHA3_512     :        Boolean;
       Raw_Bit_Streams_Only :        Boolean;
       Result               :    out RFLX.SPDM.Measurement_Hash_Algo);

   --  Select base asymmetric algorithm (DSP0274_1.1.0 [185]).
   --
   --  The arguments describe the signature algorithms supported by the requester.
   --  This function should select one of the provided algorithms.
   --
   --  @param Ctx Context.
   --  @param TPM_ALG_ECDSA_ECC_NIST_P384 ECDSA-ECC-384 supported and requested.
   --  @param TPM_ALG_RSAPSS_4096 RSAPSS-4096 supported and requested.
   --  @param TPM_ALG_RSASSA_4096 RSASSA-4096 supported and requested.
   --  @param TPM_ALG_ECDSA_ECC_NIST_P256 ECDSA-ECC-256 supported and requested.
   --  @param TPM_ALG_RSAPSS_3072 RSAPSS-3072 supported and requested.
   --  @param TPM_ALG_RSASSA_3072 RSASSA-3072 supported and requested.
   --  @param TPM_ALG_RSAPSS_2048 RSAPSS-2048 supported and requested.
   --  @param TPM_ALG_RSASSA_2048 RSASSA-2048 supported and requested.
   --  @param TPM_ALG_ECDSA_ECC_NIST_P521 ECDSA-ECC-521 supported and requested.
   --  @param Result Selected algorithm.
   overriding
   procedure Plat_Cfg_Sel_Base_Asym_Algo
      (Ctx                         : in out Context;
       TPM_ALG_ECDSA_ECC_NIST_P384 :        Boolean;
       TPM_ALG_RSAPSS_4096         :        Boolean;
       TPM_ALG_RSASSA_4096         :        Boolean;
       TPM_ALG_ECDSA_ECC_NIST_P256 :        Boolean;
       TPM_ALG_RSAPSS_3072         :        Boolean;
       TPM_ALG_RSASSA_3072         :        Boolean;
       TPM_ALG_RSAPSS_2048         :        Boolean;
       TPM_ALG_RSASSA_2048         :        Boolean;
       TPM_ALG_ECDSA_ECC_NIST_P521 :        Boolean;
       Result                      :    out RFLX.SPDM.Base_Asym_Algo);

   --  Select base hash algorithm (DSP0274_1.1.0 [185]).
   --
   --  The arguments describe the hash algorithms supported by the requester.
   --  This function should select one of the provided algorithms.
   --
   --  @param Ctx Context.
   --  @param TPM_ALG_SHA_256 SHA-256 supported and requested.
   --  @param TPM_ALG_SHA_384 SHA-384 supported and requested.
   --  @param TPM_ALG_SHA_512 SHA-512 supported and requested.
   --  @param TPM_ALG_SHA3_256 SHA3-256 supported and requested.
   --  @param TPM_ALG_SHA3_384 SHA3-384 supported and requested.
   --  @param TPM_ALG_SHA3_512 SHA3-512 supported and requested.
   --  @param Result Selected algorithm.
   overriding
   procedure Plat_Cfg_Sel_Base_Hash_Algo
      (Ctx              : in out Context;
       TPM_ALG_SHA_256  :        Boolean;
       TPM_ALG_SHA_384  :        Boolean;
       TPM_ALG_SHA_512  :        Boolean;
       TPM_ALG_SHA3_256 :        Boolean;
       TPM_ALG_SHA3_384 :        Boolean;
       TPM_ALG_SHA3_512 :        Boolean;
       Result           :    out RFLX.SPDM.Base_Hash_Algo);

   --  Select base asymmetric algorithm (DSP0274_1.1.0 [191]).
   --
   --  The arguments describe the key signature algorithms supported by the requester.
   --  This function should select one of the provided algorithms.
   --
   --  @param Ctx Context.
   --  @param Req_TPM_ALG_ECDSA_ECC_NIST_P384 ECDSA-ECC-384 supported and requested.
   --  @param Req_TPM_ALG_RSAPSS_4096 RSAPSS-4096 supported and requested.
   --  @param Req_TPM_ALG_RSASSA_4096 RSASSA-4096 supported and requested.
   --  @param Req_TPM_ALG_ECDSA_ECC_NIST_P256 ECDSA-ECC-256 supported and requested.
   --  @param Req_TPM_ALG_RSAPSS_3072 RSAPSS-3072 supported and requested.
   --  @param Req_TPM_ALG_RSASSA_3072 RSASSA-3072 supported and requested.
   --  @param Req_TPM_ALG_RSAPSS_2048 RSAPSS-2048 supported and requested.
   --  @param Req_TPM_ALG_RSASSA_2048 RSASSA-2048 supported and requested.
   --  @param Req_TPM_ALG_ECDSA_ECC_NIST_P521 ECDSA-ECC-521 supported and requested.
   --  @param Result Selected algorithm.
   overriding
   procedure Plat_Cfg_Sel_RBAA
      (Ctx                             : in out Context;
       Req_TPM_ALG_ECDSA_ECC_NIST_P384 :        Boolean;
       Req_TPM_ALG_RSAPSS_4096         :        Boolean;
       Req_TPM_ALG_RSASSA_4096         :        Boolean;
       Req_TPM_ALG_ECDSA_ECC_NIST_P256 :        Boolean;
       Req_TPM_ALG_RSAPSS_3072         :        Boolean;
       Req_TPM_ALG_RSASSA_3072         :        Boolean;
       Req_TPM_ALG_RSAPSS_2048         :        Boolean;
       Req_TPM_ALG_RSASSA_2048         :        Boolean;
       Req_TPM_ALG_ECDSA_ECC_NIST_P521 :        Boolean;
       Result                          :    out RFLX.SPDM.Base_Asym_Algo);

   --  Get digests for digests response (DSP0274_1.1.0 [232]).
   --
   --  @param Ctx Context.
   --  @param Result Digests data.
   overriding
   procedure Plat_Get_Digests_Data
      (Ctx    : in out Context;
       Result :    out RFLX.SPDM_Responder.Digests_Data.Structure);

   --  Validate an incoming certificate request (DSP0274_1.1.0 [238]).
   --
   --  @param Ctx Context.
   --  @param Slot Certificate slot number.
   --  @param Offset Certificate portion offset.
   --  @param Length Certificate portion length.
   --  @param Result Success.
   overriding
   procedure Plat_Valid_Certificate_Request
      (Ctx    : in out Context;
       Slot   :        RFLX.SPDM.Slot;
       Offset :        RFLX.SPDM.Offset;
       Length :        RFLX.SPDM.Cert_Length;
       Result :    out Boolean);

   --  Provide requested certificate chain (DSP0274_1.1.0 [238]).
   --
   --  @param Ctx Context.
   --  @param Slot Requested certificate slot.
   --  @param Offset Offset in the certificate chain.
   --  @param Length Length of the requested certificate portion.
   --  @param Result Certificate response including certificate data, portion length and
   --               remainder length (DSP0274_1.1.0 [239]).
   overriding
   procedure Plat_Get_Certificate_Response
      (Ctx    : in out Context;
       Slot   :        RFLX.SPDM.Slot;
       Offset :        RFLX.SPDM.Offset;
       Length :        RFLX.SPDM.Cert_Length;
       Result :    out RFLX.SPDM.Certificate_Response.Structure);

   --  Get the number of measurement indices (DSP0274_1.1.0 [327]).
   --
   --  Returns the number of indices available from the responder. This
   --  may be zero if none are available and up to 254.
   --
   --  @param Ctx Context.
   --  @param Result Number of measurements.
   overriding
   procedure Plat_Get_Number_Of_Indices
      (Ctx    : in out Context;
       Result :    out RFLX.SPDM.Measurement_Count);

   --  Generate a nonce for cryptographic operations.
   --
   --  The platform must always keep the latest generated nonce and shall
   --  add it to the transcript when Plat_Update_Transcript_Nonce
   --  is called. Only after this function is called the nonce can be marked
   --  as valid.
   --
   --  @param Ctx Context.
   --  @param Result Nonce containing 32 byte long buffer.
   overriding
   procedure Plat_Get_Nonce (Ctx    : in out Context;
                             Result :    out RFLX.SPDM.Nonce.Structure);

   --  Return a DMTF measurement field (DSP0274_1.1.0 [335]).
   --
   --  @param Ctx Context.
   --  @param Index Requested measurement index.
   --  @param Result Complete DMTF measurement field.
   overriding
   procedure Plat_Get_DMTF_Measurement_Field (Ctx    : in out Context;
                                              Index  :        RFLX.SPDM.Index;
                                              Result :    out RFLX.SPDM.DMTF_Measurement_Field.Structure);

   --  Provide opaque data for the measurement response (DSP0274_1.1.0 [327]).
   --
   --  @param Ctx Context.
   --  @param Result Opaque data.
   overriding
   procedure Plat_Get_Meas_Opaque_Data (Ctx    : in out Context;
                                        Result :    out RFLX.SPDM_Responder.Opaque_Data.Structure);

   --  Register a new transcript with the platform.
   --
   --  The returned ID must be used on all subsequent operations on the same transcript.
   --
   --  @param Ctx Context.
   --  @param Kind Transcript kind.
   --  @param Result Transcript ID. On success Plat_Valid_Transcript_ID
   --         must return true on this ID.
   overriding
   procedure Plat_Get_New_Transcript (Ctx    : in out Context;
                                      Kind   :        RFLX.SPDM_Responder.Transcript_Kind;
                                      Result :    out RFLX.SPDM_Responder.Transcript_ID);

   --  Indicate whether a transcript ID is valid.
   --
   --  @param Ctx Context.
   --  @param Transcript Transcript ID.
   --  @param Result True if transcript ID is valid.
   overriding
   procedure Plat_Valid_Transcript_ID (Ctx        : in out Context;
                                       Transcript :        RFLX.SPDM_Responder.Transcript_ID;
                                       Result     :    out Boolean);

   --  Reset an already registered transcript.
   --
   --  This operation may change the transcript kind, too. The returned transcript ID
   --  may be different from the provided one. It has the same behaviour as
   --  Plat_Get_New_Transcript except that it allows reusing an existing resource.
   --
   --  @param Ctx Context.
   --  @param Transcript Old transcript ID.
   --  @param Kind Transcript kind for the new transcript.
   --  @param Result New transcript ID.
   overriding
   procedure Plat_Reset_Transcript (Ctx        : in out Context;
                                    Transcript :        RFLX.SPDM_Responder.Transcript_ID;
                                    Kind       :        RFLX.SPDM_Responder.Transcript_Kind;
                                    Result     :    out RFLX.SPDM_Responder.Transcript_ID);

   --  Append a chunk of data to the transcript.
   --
   --  @param Ctx Context.
   --  @param Transcript Transcript ID.
   --  @param Data Transcript data to be appended.
   --  @param Offset Offset in data.
   --  @param Length Length of data to be appended to the transcript,
   --                Length + Offset is less or equal to the size of data.
   --  @param Result Success.
   overriding
   procedure Plat_Update_Transcript (Ctx        : in out Context;
                                     Transcript :        RFLX.SPDM_Responder.Transcript_ID;
                                     Data       :        RFLX.RFLX_Types.Bytes;
                                     Offset     :        RFLX.SPDM.Length_16;
                                     Length     :        RFLX.SPDM.Length_16;
                                     Result     :    out Boolean);

   --  Append the latest generated nonce to the transcript.
   --
   --  Append the latest nonce generated by Plat_Get_Nonce to the
   --  transcript. The nonce must be marked as invalid after this operation.
   --  If the nonce is already marked as invalid when this function is called
   --  the operation must fail.
   --
   --  @param Ctx Context.
   --  @param Transcript Transcript ID.
   --  @param Result Success.
   overriding
   procedure Plat_Update_Transcript_Nonce (Ctx        : in out Context;
                                           Transcript :        RFLX.SPDM_Responder.Transcript_ID;
                                           Result     :    out Boolean);

   --  Generate signature from current transcript.
   --
   --  Generate the signature from the current state of the transcript. This
   --  does not invalidate or reset the transcript.
   --
   --  @param Ctx Context.
   --  @param Transcript Transcript ID.
   --  @param Slot Slot ID of the signing key.
   --  @param Result Signature. In case of an error Result.Length must be set to 0.
   overriding
   procedure Plat_Get_Signature (Ctx        : in out Context;
                                 Transcript :        RFLX.SPDM_Responder.Transcript_ID;
                                 Slot       :        RFLX.SPDM.Slot;
                                 Result     :    out RFLX.SPDM_Responder.Signature.Structure);

   --  Initialization function for empty signatures.
   --
   --  This is a helper function for the specification and does not interact with any
   --  platform code. It must not be changed. if a custom implementation is done it must
   --  return a Signature with the requested length and zero initialized data.
   --
   --  @param Ctx Context.
   --  @param Length Length of the zeroed signature.
   --  @param Result Zeroed out signature data.
   overriding
   procedure Null_Signature (Ctx    : in out Context;
                             Length :        RFLX.SPDM.Signature_Length;
                             Result :    out RFLX.SPDM_Responder.Signature.Structure);
end SPDM_C_Responder;
