# SINR Parity Benchmark Report

**Generated:** 2026-04-13T11:41:48.411007

## Overall Statistics

| Metric | Value |
|--------|-------|
| Total Samples | 10000 |
| Mean SINR | 39.66 dB |
| Median SINR | 37.86 dB |
| Std Dev | 13.96 dB |
| Min | 5.21 dB |
| Max | 80.00 dB |
| P95 | 66.15 dB |

## Statistics by Link Type

### V2V

| Metric | Value |
|--------|-------|
| Count | 6576 |
| Mean SINR | 43.62 dB |
| Median | 41.45 dB |
| Std Dev | 13.56 dB |
| Min | 9.13 dB |
| Max | 80.00 dB |
| P95 | 68.45 dB |

### RSU2V

| Metric | Value |
|--------|-------|
| Count | 1712 |
| Mean SINR | 32.05 dB |
| Median | 31.03 dB |
| Std Dev | 11.31 dB |
| Min | 5.21 dB |
| Max | 62.77 dB |
| P95 | 52.55 dB |

### V2RSU

| Metric | Value |
|--------|-------|
| Count | 1712 |
| Mean SINR | 32.05 dB |
| Median | 31.03 dB |
| Std Dev | 11.31 dB |
| Min | 5.21 dB |
| Max | 62.77 dB |
| P95 | 52.55 dB |

## Statistics by Distance Band

### 0-100m

| Metric | Value |
|--------|-------|
| Count | 1776 |
| Mean SINR | 59.40 dB |
| Median | 60.74 dB |
| Std Dev | 10.72 dB |
| Min | 31.65 dB |
| Max | 80.00 dB |

### 100-250m

| Metric | Value |
|--------|-------|
| Count | 1700 |
| Mean SINR | 46.31 dB |
| Median | 46.41 dB |
| Std Dev | 8.52 dB |
| Min | 19.62 dB |
| Max | 64.17 dB |

### 250-500m

| Metric | Value |
|--------|-------|
| Count | 4400 |
| Mean SINR | 35.45 dB |
| Median | 36.21 dB |
| Std Dev | 7.87 dB |
| Min | 9.13 dB |
| Max | 53.13 dB |

### >500m

| Metric | Value |
|--------|-------|
| Count | 2124 |
| Mean SINR | 26.57 dB |
| Median | 27.00 dB |
| Std Dev | 8.11 dB |
| Min | 5.21 dB |
| Max | 48.00 dB |

## Notes

- **Secondary SINR**: C++ implementation in MyRSUApp (exported to Redis/database)
- **Validation Criteria**: Mean error <3 dB, bias <2 dB
- **Link Types**: V2V, V2RSU, RSU2V, RSU2RSU
- **Distance Bands**: 0-100m, 100-250m, 250-500m, >500m
