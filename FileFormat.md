# File Structure


## Job Description
**D7_12345.job**

| Name | Type | Bytes |
| --- | --- | --- |
| Variables | u32 | 4 |
| numTops | u32 | 4 |
| tops | Top[numTops] | numTops * N |


### Top
| Name | Type | Bytes |
| --- | --- | --- |
| Index | u32 | 4 |
| top | MBF | 1-16 |



## Results File
**D7_12345.jobResult**

| Name | Type | Bytes |
| --- | --- | --- |
| timestamp | u64 | 8 |
| totalTime | f32 | 4 |
| occupation | f32 | 4 |
| numTops | u32 | 4 |
| results | Result[numTops] | numTops * M |

### Result
| Name | Type | Bytes |
| --- | --- | --- |
| Index | u32 | 4 |
| top | MBF | 1-16 |
| betaSum | u128 | 16 |
| countedIntervalSizeDown | u64 | 8 |

