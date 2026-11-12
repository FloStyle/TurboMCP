# NOV09-20 integration review

Implemented: c7635e357f7a70e778f21a56e9d1aa45658d34b2. Approved base: 38a0133629beab290638a3dbbdd66429ce4669c2. Scope gate passed for the two approved files.

Known loaded non-Blueprint asset classes now report not applicable when no generated-class evidence exists. Loaded Blueprint and registry tag paths retain precedence; unresolved classes remain unknown. Stock 5.8.4 AssetData.h confirms GetClass defaults to EResolveClass::No, so this change does not load classes or assets.

Testing: untested at runtime. Added a stock-material assertion to the existing metadata workflow; no build or editor run for this small reporting change. Batch the focused workflow with the next incremental build. Existing tests cover unloaded Blueprint descriptions; equality across two calls is not proof of no first-call loading. Source inspection supplies the non-loading API evidence, not that assertion. No full regression claimed.
