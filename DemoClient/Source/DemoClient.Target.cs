using UnrealBuildTool;
using System.Collections.Generic;

public class DemoClientTarget : TargetRules
{
	public DemoClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DemoClient");
	}
}
