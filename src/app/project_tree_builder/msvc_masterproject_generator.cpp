#include <app/project_tree_builder/msvc_masterproject_generator.hpp>


#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>

BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------


CMsvcMasterProjectGenerator::CMsvcMasterProjectGenerator(
                                                  const CProjectItemsTree& tree,
                                                  const list<string>& configs,
                                                  const string& project_dir)
    :m_Tree          (tree),
     m_Configs       (configs),
	 m_Name          ("_MasterProject"),
     m_ProjectDir    (project_dir),
     m_ProjectItemExt(".build")
{
    m_CustomBuildCommand  = "@echo on\n";
    m_CustomBuildCommand += "devenv "\
                            "/build $(ConfigurationName) "\
                            "/project $(InputName) "\
                            "\"$(SolutionPath)\"\n";
}

CMsvcMasterProjectGenerator::~CMsvcMasterProjectGenerator(void)
{
}


string CMsvcMasterProjectGenerator::ConfigName(const string& config) const
{
    return config +'|'+ MSVC_PROJECT_PLATFORM;
}


void CMsvcMasterProjectGenerator::SaveProject(const string& base_name)
{
    CVisualStudioProject xmlprj;
    
    {{
        //Attributes:
        xmlprj.SetAttlist().SetProjectType  (MSVC_PROJECT_PROJECT_TYPE);
        xmlprj.SetAttlist().SetVersion      (MSVC_PROJECT_VERSION);
        xmlprj.SetAttlist().SetName         (m_Name);
        xmlprj.SetAttlist().SetRootNamespace(MSVC_MASTERPROJECT_ROOT_NAMESPACE);
        xmlprj.SetAttlist().SetKeyword      (MSVC_MASTERPROJECT_KEYWORD);
    }}
    
    {{
        //Platforms
         CRef<CPlatform> platform(new CPlatform(""));
         platform->SetAttlist().SetName(MSVC_PROJECT_PLATFORM);
         xmlprj.SetPlatforms().SetPlatform().push_back(platform);
    }}

    ITERATE(list<string>, p , m_Configs) {
        // Iterate all configurations
        const string& config = *p;
        
        CRef<CConfiguration> conf(new CConfiguration());

#define SET_ATTRIBUTE( node, X, val ) node->SetAttlist().Set##X(val)        

        {{
            //Configuration
            SET_ATTRIBUTE(conf, Name,     ConfigName(config));

            SET_ATTRIBUTE(conf, OutputDirectory,        
                                         "$(SolutionDir)$(ConfigurationName)" );
            
            SET_ATTRIBUTE(conf, IntermediateDirectory,  
                                         "$(ConfigurationName)" );
            
            SET_ATTRIBUTE(conf, ConfigurationType,
                                         "10" );
            
            SET_ATTRIBUTE(conf, CharacterSet,
                                         "2" );
            
            SET_ATTRIBUTE(conf, ManagedExtensions,
                                        "TRUE" );
        }}

        {{
            //VCCustomBuildTool
            CRef<CTool> tool(new CTool(""));
            SET_ATTRIBUTE(tool, Name, "VCCustomBuildTool" );
            conf->SetTool().push_back(tool);
        }}
        {{
            //VCMIDLTool
            CRef<CTool> tool(new CTool(""));
            SET_ATTRIBUTE(tool, Name, "VCMIDLTool" );
            conf->SetTool().push_back(tool);
        }}
        {{
            //VCPostBuildEventTool
            CRef<CTool> tool(new CTool(""));
            SET_ATTRIBUTE(tool, Name, "VCPostBuildEventTool" );
            conf->SetTool().push_back(tool);
        }}
        {{
            //VCPreBuildEventTool
            CRef<CTool> tool(new CTool(""));
            SET_ATTRIBUTE(tool, Name, "VCPreBuildEventTool" );
            conf->SetTool().push_back(tool);
        }}

        xmlprj.SetConfigurations().SetConfiguration().push_back(conf);
    }

    {{
        //References
        xmlprj.SetReferences("");
    }}

    {{
        //Filters
        list<string> root_ids;
        m_Tree.GetRoots(&root_ids);
        ITERATE(list<string>, p, root_ids) {

            const string& root_id = *p;
            ProcessProjectBranch(root_id, &xmlprj.SetFiles());
        }
    }}

    {{
        //Globals
        xmlprj.SetGlobals("");
    }}


    string project_path = CDirEntry::ConcatPath(m_ProjectDir, base_name);

    project_path += ".vcproj";

    SaveToXmlFile(project_path, xmlprj);
}


void CMsvcMasterProjectGenerator::ProcessProjectBranch(const string& project_id,
                                                       CSerialObject * pParent)
{
    if ( IsAlreadyProcessed(project_id) )
        return;

    CRef<CFilter> project_filter = FindOrCreateFilter(project_id, pParent);

    list<string> sibling_ids;
    m_Tree.GetSiblings(project_id, &sibling_ids);
    ITERATE(list<string>, p, sibling_ids) {
        //continue recursive lookup
        const string& sibling_id = *p;
        ProcessProjectBranch(sibling_id, &(*project_filter));
    }

    AddProjectToFilter(project_filter, project_id);
}


static void s_RegisterCreatedFilter(
                                 CRef<CFilter>& filter, CSerialObject * pParent)
{
    {{
        CFiles * pFiles = dynamic_cast<CFiles *>(pParent);
        if (pFiles != NULL) {
            // Parent is <Files> section of MSVC project
            pFiles->SetFilter().push_back(filter);
            return;
        }
    }}
    {{
        CFilter * pFilter = dynamic_cast<CFilter *>(pParent);
        if (pFilter != NULL) {
            // Parent is another Filter (folder)
            CRef< CFilter_Base::C_FF::C_E > ce( new CFilter_Base::C_FF::C_E() );
            ce->SetFilter(*filter);
            pFilter->SetFF().SetFF().push_back(ce);
            return;
        }
    }}
}


CRef<CFilter> CMsvcMasterProjectGenerator::FindOrCreateFilter(
                                                       const string& project_id,
                                                       CSerialObject * pParent)
{
    CProjectItemsTree::TProjects::const_iterator p = 
                                             m_Tree.m_Projects.find(project_id);

    if (p != m_Tree.m_Projects.end()) {
        //Find filter for the project or create a new one
        const CProjItem& project = p->second;
        
        TFiltersCache::iterator n = m_FiltersCache.find(project.m_SourcesBaseDir);
        if (n != m_FiltersCache.end())
            return n->second;
        
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName(GetFolder(project.m_SourcesBaseDir));
        filter->SetAttlist().SetFilter("");

        m_FiltersCache[project.m_SourcesBaseDir] = filter;
        s_RegisterCreatedFilter(filter, pParent);
        return filter;
    }
    else {
        //TODO - reconsider
        LOG_POST("||||||||| No project with id : " + project_id);
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Unknown");
        filter->SetAttlist().SetFilter("");
        s_RegisterCreatedFilter(filter, pParent);
        return filter;
    }
}


bool CMsvcMasterProjectGenerator::IsAlreadyProcessed(
                                                 const string& project_id)
{
    set<string>::const_iterator p = m_ProcessedIds.find(project_id);
    if (p == m_ProcessedIds.end()) {
        m_ProcessedIds.insert(project_id);
        return false;
    }

    return true;
}


void CMsvcMasterProjectGenerator::AddProjectToFilter(CRef<CFilter>& filter, 
                                                     const string& project_id)
{
    CProjectItemsTree::TProjects::const_iterator p = 
                                             m_Tree.m_Projects.find(project_id);
    if (p != m_Tree.m_Projects.end()) {
        // Add project to this filter (folder)
        const CProjItem& project = p->second;

        CRef<CFFile> file(new CFFile());
        file->SetAttlist().SetRelativePath(project.m_ID + m_ProjectItemExt);
        
        CreateProjectFileItem(project);

        ITERATE(list<string>, n , m_Configs) {
            // Iterate all configurations
            const string& config = *n;

            CRef<CFileConfiguration> file_config(new CFileConfiguration());
            file_config->SetAttlist().SetName(ConfigName(config));

            CRef<CTool> custom_build(new CTool(""));
            custom_build->SetAttlist().SetName("VCCustomBuildTool");
            custom_build->SetAttlist().SetDescription(
                                            "Building project : $(InputName)");
            custom_build->SetAttlist().SetCommandLine(m_CustomBuildCommand);
            custom_build->SetAttlist().SetOutputs("$(InputPath).aanofile.out");
            file_config->SetTool(*custom_build);

            file->SetFileConfiguration().push_back(file_config);
        }
        CRef< CFilter_Base::C_FF::C_E > ce( new CFilter_Base::C_FF::C_E() );
        ce->SetFile(*file);
        filter->SetFF().SetFF().push_back(ce);
    }
    else {
        
        LOG_POST("||||||||| No project with id : " + project_id);
    }
}


void CMsvcMasterProjectGenerator::CreateProjectFileItem(
                                                       const CProjItem& project)
{
    string file_path = CDirEntry::ConcatPath(m_ProjectDir, project.m_ID);
    file_path += m_ProjectItemExt;

    CNcbiOfstream  ofs(file_path.c_str(), ios::out | ios::trunc);
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);
}

//------------------------------------------------------------------------------
END_NCBI_SCOPE
