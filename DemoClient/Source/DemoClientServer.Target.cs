using UnrealBuildTool;
using System.Collections.Generic;

public class DemoClientServerTarget : TargetRules
{
	public DemoClientServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DemoClient");
	}
}
