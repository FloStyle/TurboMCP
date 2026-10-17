import pathlib,importlib.util,re,json,hashlib,subprocess,collections
R=pathlib.Path(__file__).resolve().parents[1];S=R/'Plugins/TurboMCP/Source/TurboMCP'
s=importlib.util.spec_from_file_location('coverage',R/'Tools/audit-tool-coverage.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
decls=m.read_declarations();calls=m.read_calls();src={p:p.read_text(encoding='utf8') for p in S.rglob('*.cpp')};tools=[]
for name,d in sorted(decls.items()):
 matches=[]
 for p,t in src.items():
  for found in re.finditer(r'FString\s+'+re.escape(d['owner'])+'::'+re.escape(name)+r'\s*\(',t):matches.append({'path':p.relative_to(R).as_posix(),'line':t[:found.start()].count('\n')+1})
 c=calls.get(name,{})
 family=('graph' if any(x in name for x in ['Graph','Node','LocalVars','Layout','Comment']) else 'material' if 'Material' in name else 'widget' if 'Widget' in name else 'schema-and-observability' if any(x in name for x in ['Status','Tool','CallLog','CallCost','Trace','PlayCheck']) else 'asset-and-data')
 overlap='compare-native-before-expanding' if any(x in name for x in ['Enum','Struct','Vcs','Widget']) else 'retain-existing-safety-and-workflow-value'
 tools.append({'id':name,'toolset':d['owner'],'parameters':d['params'],'family':family,'definitions':matches,'test_references':['Tools/'+x for x in sorted(c.get('files',[]))],'static_call_evidence':'parsed_call' if c.get('parsed') or c.get('in_call') else 'name_reference_only' if c else 'missing','implementation_evidence':'definition_located' if len(matches)==1 else 'review_required','runtime_evidence':'not_certified','release_disposition':'experimental_pending_acceptance','investment_decision':overlap})
report={'schema':1,'plugin_tree':subprocess.check_output(['git','-C',str(R),'rev-parse','HEAD:Plugins/TurboMCP'],text=True).strip(),'scope':'All AICallable declarations in the single toolset header, qualified C++ definition locations, and static Python test references. Not a semantic or runtime certification.','counts':{'tools':len(tools),'by_toolset':dict(collections.Counter(x['toolset'] for x in tools))},'tools':tools}
outputs={'docs/audit/capabilities.json':json.dumps(report,indent=2)+'\n'}
lines=['# Capability evidence inventory','','Generated from declarations, definitions and static test references. All runtime rows remain unaccepted until observed; no release capability is inferred from a name.','','| Tool | Definition | Static call evidence | Runtime | Investment |','| --- | --- | --- | --- | --- |']
for x in tools:
 d=x['definitions'];ref='; '.join('['+pathlib.Path(v['path']).name+'](../../'+v['path']+')' for v in d) or 'MISSING'
 lines.append('| '+x['id']+' | '+ref+' | '+x['static_call_evidence']+' | not certified | '+x['investment_decision']+' |')
outputs['docs/audit/capabilities.md']='\n'.join(lines)+'\n'
for rel,text in outputs.items():
    p=R/rel
    if '--check' in __import__('sys').argv:
        if not p.exists() or p.read_text(encoding='utf8')!=text:raise SystemExit('Stale capability inventory: run python Tools/audit-inventory.py')
    else:
        p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text,encoding='utf8',newline='\n')
print('Tools',len(tools),'definitions',collections.Counter(len(x['definitions']) for x in tools),'call evidence',collections.Counter(x['static_call_evidence'] for x in tools))
