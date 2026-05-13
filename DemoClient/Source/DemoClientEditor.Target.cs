using UnrealBuildTool;
using System.Collections.Generic;

public class DemoClientEditorTarget : TargetRules
{
	public DemoClientEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DemoClient");
	}
}
