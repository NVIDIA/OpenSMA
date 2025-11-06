/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <span>
#include "fsl_flexio_sgpio_target.h"
#include "fsl_flexio_sgpio_edma.h"

namespace sys::sgpio {

class Driver
{
protected:
    FLEXIO_SPI_Type                _spi_dev;
    flexio_spi_slave_edma_handle_t _spi_handle;
    edma_handle_t                  _tx_handle;
    edma_handle_t                  _rx_handle;
};

}  // namespace sys::sgpio
