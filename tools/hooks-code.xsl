<?xml version="1.0"?>
<xsl:stylesheet version="1.1" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="text"/>
  <xsl:template match="/hooks">

<!-- header -->
<xsl:text><![CDATA[
// Generated by hooks-code.xsl
// DO NOT EDIT

#include "mud/room.h"
#include "mud/exit.h"
#include "mud/object.h"
#include "mud/zone.h"
#include "common/gcvector.h"
#include "mud/char.h"
#include "mud/player.h"
#include "mud/npc.h"
#include "scriptix/function.h"

namespace Hooks {
]]></xsl:text>

<!-- hook ids -->
<xsl:for-each select="hook">
	<xsl:text>namespace { GCType::vector&lt;Scriptix::ScriptFunction&gt; </xsl:text><xsl:value-of select="@name" /><xsl:text>_cb; }
bool </xsl:text><xsl:value-of select="@name" /><xsl:text> (</xsl:text>
	<xsl:for-each select="arg">
		<xsl:if test="position()!=1">
			<xsl:text>, </xsl:text>
		</xsl:if>

		<xsl:choose>
			<xsl:when test="@type='String'">
				<xsl:text>StringArg </xsl:text>
			</xsl:when>
			<xsl:when test="@type='Integer'">
				<xsl:text>long </xsl:text>
			</xsl:when>
			<xsl:when test="@type='Boolean'">
				<xsl:text>bool </xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="@type" />
				<xsl:text>* </xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>_</xsl:text><xsl:value-of select="@name" />
	</xsl:for-each>
	<xsl:text>) {
	Scriptix::Value argv[</xsl:text><xsl:value-of select="count(arg)" /><xsl:text>];

	if (</xsl:text><xsl:value-of select="@name" /><xsl:text>_cb.empty())
		return false;

	</xsl:text><xsl:for-each select="arg">
		<xsl:text>argv[</xsl:text><xsl:value-of select="position()-1" /><xsl:text>] = _</xsl:text><xsl:value-of select="@name" /><xsl:text>;</xsl:text>
	</xsl:for-each><xsl:text>

	for (GCType::vector&lt;Scriptix::ScriptFunction&gt;::iterator i = </xsl:text><xsl:value-of select="@name" /><xsl:text>_cb.begin(); i != </xsl:text><xsl:value-of select="@name" /><xsl:text>_cb.end(); ++i) {
		Scriptix::ScriptManager.invoke(*i, </xsl:text><xsl:value-of select="count(arg)" /><xsl:text>, argv, NULL);
	}

	return true;
}
</xsl:text>
</xsl:for-each>

<xsl:text>
int add (StringArg name, Scriptix::ScriptFunction cb) {
</xsl:text>
	<xsl:for-each select="hook">
		<xsl:text>if (name == "</xsl:text><xsl:value-of select="@name" /><xsl:text>") { </xsl:text><xsl:value-of select="@name" /><xsl:text>_cb.push_back(cb); return 0; }</xsl:text>
	</xsl:for-each>
<xsl:text>
	return 1;
}
</xsl:text>

<!-- footer -->
<xsl:text><![CDATA[

} // namespace Hooks
]]></xsl:text>

  </xsl:template>
</xsl:stylesheet>
