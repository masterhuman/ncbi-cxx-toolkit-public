<?xml version="1.0"?>
<xsl:stylesheet
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:math="http://www.w3.org/2005/xpath-functions/math"
	xmlns:xs="http://www.w3.org/2001/XMLSchema"
	exclude-result-prefixes="xs math" version="3.0">
	<xsl:output indent="yes" omit-xml-declaration="yes" />

	<xsl:param name="input" select="'input.json'" />

	<!-- convert JSON to HTML page -->
	<xsl:template name="main" >
		<html>
			<header>
				<title>PubSeq Gateway - Help Page</title>
				<style>
					table, th, td {
						border: 1px solid;
						padding: 10px;
						}
					table {
						border-collapse: collapse;
						}
					th {
						background-color: #D8D8D8;
						font-size: larger;
						font-weight: bold;
					}
					table.first-column > tbody > tr > td:nth-of-type(1) {
						font-size: larger;
						font-weight: bold;
						background-color: #F5F5F5;
					}
					table.nested td {
						border-color: silver;
					}
					
				</style>
			</header>		
			<body>
				<xsl:apply-templates select="json-to-xml(unparsed-text($input))" />
			</body>
		</html>		
	</xsl:template>
	
	<!-- generate body of HTML page -->
	
	<xsl:template match="map" xpath-default-namespace="http://www.w3.org/2005/xpath-functions">	
		<xsl:call-template name="doAbout">
			<xsl:with-param name="nodeAbout" select="map[@key='about']" />
		</xsl:call-template>
		<xsl:call-template name="doTOC">
			<xsl:with-param name="nodeRequests" select="map[@key='requests']" />
		</xsl:call-template>		
		<xsl:call-template name="doRequests">
			<xsl:with-param name="nodeRequests" select="map[@key='requests']" />
		</xsl:call-template>	
		<xsl:call-template name="doRefs">
			<xsl:with-param name="nodeRefs" select="array[@key='references']" />
		</xsl:call-template>			
	</xsl:template>
	
	<!-- display "about"  section -->
	<xsl:template name="doAbout" xpath-default-namespace="http://www.w3.org/2005/xpath-functions">
	  <xsl:param name="nodeAbout" />
		<h1>
			<xsl:value-of select="$nodeAbout/string[@key='name']" />
		</h1>
		<p>
			<xsl:value-of select="$nodeAbout/string[@key='description']" />
		</p>
		<!-- version table -->
		<table>
				<xsl:for-each select="$nodeAbout/string[not(@key='name') and not(@key='description')]">
				  <tr><td><xsl:value-of select="@key"/></td><td><xsl:value-of select="."/></td></tr>
				</xsl:for-each>
		</table>
			<br/>
			<hr/>
			<br/>
			<br/>		
	</xsl:template>
	
	<!-- display TOC -->
	<xsl:template name="doTOC" xpath-default-namespace="http://www.w3.org/2005/xpath-functions">
	 <xsl:param name="nodeRequests" />
	 <h2>Commands</h2>
		<ul>
			<xsl:for-each select="$nodeRequests/map">
				<li><a><xsl:attribute name="href"><xsl:value-of select="concat('#', translate(@key, '/', '_'))"/></xsl:attribute><xsl:value-of select="@key"/></a></li>	 
			</xsl:for-each>
		</ul>
	</xsl:template>
	

	<!-- display a list of requests -->
	<xsl:template name="doRequests" xpath-default-namespace="http://www.w3.org/2005/xpath-functions">
	  <xsl:param name="nodeRequests" />
				<xsl:for-each select="$nodeRequests/map">
				  <a><xsl:attribute name="id"><xsl:value-of select="translate(@key, '/', '_')"/></xsl:attribute></a>
				  <h2><xsl:value-of select="@key"/></h2>
				  <p><xsl:value-of select="string[@key='description']"/></p>
				  
				  <xsl:call-template name="doParameters">
				    <xsl:with-param name="nodeParameters" select="map[@key='parameters']"/>
				  </xsl:call-template>

				  <h3>Reply</h3>
				  <p><xsl:value-of select="map[@key='reply']/string[@key='description']"/></p>

					<hr/>
					<br/>
					<br/>				  
				  
				</xsl:for-each>
	  
	</xsl:template>
	
	<!-- display parameters of a request in a table -->
	<xsl:template name="doParameters" xpath-default-namespace="http://www.w3.org/2005/xpath-functions">
		<xsl:param name="nodeParameters" />
		<xsl:if test="count($nodeParameters/map) &gt; 0">
		<table class="first-column">
				<!-- headers -->
				<tr><th>Parameter</th><th>Optional</th><th>Type</th><th>Description</th><th>Allowed Values</th><th>Default</th></tr>
				<xsl:for-each select="$nodeParameters/map">
				<tr>
				
				<!-- param name -->
				<td><xsl:value-of select="@key"/></td>
				
				<!-- mandatory/optional -->
				<td>
				  <xsl:choose>
				    <xsl:when test="boolean[@key='mandatory']='true'">mandatory</xsl:when>
					<xsl:otherwise>optional</xsl:otherwise>
				  </xsl:choose>
				</td>
				
				<!-- data type -->
				<td><xsl:value-of select="string[@key='type']"/></td>
				
				
				<!-- allowed values -->
				<td class="description">
				<xsl:choose>
				  <xsl:when test="map[@key='description']">
					<xsl:call-template name="descriptionTable">
						<xsl:with-param name="descNode" select="map[@key='description']" />
					</xsl:call-template>
				  </xsl:when>
				  <xsl:otherwise><xsl:value-of select="string[@key='description']"/></xsl:otherwise>
				</xsl:choose>
				</td>
				
				<!-- description -->
				<td><xsl:value-of select="string[@key='allowed values']"/></td>

				<!-- default value -->
				<td><xsl:value-of select="string[@key='default']"/></td>

				
				<!-- all other fields -->
				<!-- xsl:for-each select="*">
				  <td><xsl:value-of select="." /></td>
				</xsl:for-each -->
				
				</tr>
				</xsl:for-each>
		</table>
		</xsl:if>
	</xsl:template>
	
	
	<!-- display a list of references -->
	<xsl:template name="doRefs" xpath-default-namespace="http://www.w3.org/2005/xpath-functions">
		<xsl:param name="nodeRefs" />
		<h2>References</h2>
		<ol>
			<xsl:for-each select="$nodeRefs/map">
				<li><a href="{string[@key='link']}">
				<xsl:value-of select="string[@key='description']" />
				</a></li>
			</xsl:for-each>
		</ol>
	</xsl:template>

	<!-- display parameter description as a nested table -->
	<xsl:template name="descriptionTable" xpath-default-namespace="http://www.w3.org/2005/xpath-functions">
		<xsl:param name="descNode" />
		<xsl:value-of select="$descNode/string[@key='title']" />
		<br/><br/>
		<xsl:if test="$descNode/array[@key='table']">
			<table class="nested">
				<xsl:for-each select="$descNode/array[@key='table']/array">
				  <tr>
					<xsl:for-each select="string">
					  <td><xsl:value-of select="." /></td>
					</xsl:for-each>
				  </tr>
				</xsl:for-each>
			</table>
		</xsl:if>
	</xsl:template>
	
	
</xsl:stylesheet>

