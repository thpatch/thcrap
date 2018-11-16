// /SAFESEH compliance. Note that the @feat.00 symbol *must not* be global.
//
// The MSBuild GAS target uses the C preprocessor to automatically inject this
// file into every assembled file by default; to disable this, set the
// <SAFESEH> MSBuild property to `false`.
	.set @feat.00, 1
