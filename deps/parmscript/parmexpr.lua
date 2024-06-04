local fmt=string.format
local map=function(f,t)
  local result={}
  for _,v in ipairs(t) do
    table.insert(result, f(v))
  end
  return result
end
local reduce=function(f,t,init)
  if #t==1 and init~=nil then
    return f(init, t[1])
  elseif #t<2 then
    return
  end
  local result=init~=nil and init or t[1]
  for i=init~=nil and 1 or 2, #t do
    result=f(result,t[i])
  end
  return result
end
local titleize=function(t)
  return string.gsub(t, '%w+', function(w) return w:sub(1,1):upper()..w:sub(2) end)
end
local dbgf=function(...)end
--dbgf=function(...)print(fmt(...))end

local loadParmScript=function(parmscript)
  local parmsetname = 'Parms'
  --[[
  fields: tree of parms

  each parm can be a leaf (field)
  or a branch (group or list)
  ]]
  local root = {name='', path='', type='struct', fields={}}
  local parmlut = {['']=root}
  local currentbranch = root -- root
  local path = '' -- `pwd` equivalent for current parm
  local objpath = {root} -- `pwd` of parms objects
  local typedefs={string='std::string'}

  local escape = function(name)
    name = string.gsub(name, '[^%w_]', '_')
    if name=='' or not string.match(name, '[%a_]%w*') then
      name = '_'..name
    end
    return name
  end

  local quoteString=function(str)
    return fmt('%q',str):gsub('\\\n','\\n')
    --[[
    return fmt('"%s"',
      string.gsub(
        str,
        '[%c"\']',
        function(c) return fmt('""\\x%02x""', string.byte(c)) end))
    ]]
  end

  local function fullpath(name)
    if path=='' then
      return name
    else
      return path..'.'..name
    end
  end

  local defineNonField=function(ui)
    return function(label)
      local fullname=fullpath('ui_'..ui)
      local field={name='ui_only', path=fullname, parent=currentbranch, type='', ui=ui}
      table.insert(currentbranch.fields, field)
      field.meta = {label = label}
      return function(meta)
        meta.label = label
        field.meta = meta
      end
    end
  end

  local defineField=function(t, ui)
    return function(name)
      assert(not string.find(name,'[^%w_]'), fmt("name should contain letters, digits and underscores only, got \"%s\".", name))
      local fullname=fullpath(name)
      local field={name=name, path=fullname, parent=currentbranch, type=t, ui=ui, meta={}}
      table.insert(currentbranch.fields, field)
      if t and t~='' then
        assert(not parmlut[fullname], fmt('%q already exist', fullname))
        parmlut[fullname]=field
      end
      return function(meta)
        field.meta = meta
      end
    end
  end

  local enter=function(name, type, flat) -- enters dir, if flat then its fields are defined in the scope of parent
    dbgf('entering %s', name)
    local prevbranch = currentbranch
    assert(not string.find(name,'[^%w_]'), "name should contain letters, digits and underscores only.")
    if not flat then
      path=fullpath(name)
      assert(not parmlut[path], fmt('%s already exist', path))
    end
    local label=titleize(name)
    name = escape(name)
    currentbranch = {name=name, ui=type, type=type, path=path, flat=flat, parent=currentbranch, meta={label=label}, fields={}}
    if flat then
      parmlut[fullpath(name)] = currentbranch
    else
      parmlut[path] = currentbranch
    end
    table.insert(prevbranch.fields, currentbranch)
    table.insert(objpath, currentbranch)
    dbgf('path=%s, top=%s', path, currentbranch)
    return function(meta)
      currentbranch.meta = meta
    end
  end

  local leave=function(name)
    dbgf('leaving %s', name)
    if not currentbranch.flat then
      local lastdot = string.find(path, '%.[^.]+$')
      local leaf
      if lastdot then
        leaf = string.sub(path, lastdot+1)
        path=string.sub(path, 1, lastdot-1)
      else
        leaf = path
        path = ''
      end
      assert(leaf==name, 'enter/leave scope name mismatch')
    end

    assert(#objpath>1)
    local popobj=table.remove(objpath)
    assert(popobj.name==name, fmt('enter(%q)/leave(%q) scope name mismatch', popobj.name, name))
    --if not currentbranch.flat then
    --  dbgf('objpath=%s', table.concat(map(function(t) return t.name end, objpath), ' . '))
    --  assert(objpath[#objpath]==parmlut[path], fmt('%q mismatch with %q in lut', path, objpath[#objpath].path))
    --end
    currentbranch=objpath[#objpath]
    dbgf('path=%s', path)
  end

  local makeMenu=function(name)
    return defineField('menu', 'menu')(name)
  end

  local safeenv = {
    yes = true,
    no  = false,

    parmset=function(name) parmsetname=name end,
    label=defineNonField('label'),
    separator=defineNonField('separator'),
    spacer=defineNonField('spacer'),
    toggle=defineField('bool', 'toggle'),
    int=defineField('int'),
    int2=defineField('int2'),
    float=defineField('float'),
    float2=defineField('float2'),
    float3=defineField('float3'),
    float4=defineField('float4'),
    double=defineField('double'),
    color=defineField('color'),
    text=defineField('string', 'text'),
    button=defineField('function','button'),
    menu=makeMenu, -- defineField('int', 'select'),
    combo=defineField('string', 'combo'),
    group=function(name) return enter(name, 'group', true) end,
    endgroup=leave,
    struct=function(name) return enter(name, 'struct') end,
    endstruct=leave,
    list=function(name) return enter(name, 'list') end,
    endlist=leave,
    pairs=pairs,
    ipairs=ipairs,
    alias=function(underlaying, inspector_tag, initial_meta)
      return function(name)
        local meta = {inspector=inspector_tag}
        if initial_meta then
          for i,v in pairs(initial_meta) do
            meta[i] = v
          end
        end
        local metasetter = underlaying(name)
        metasetter(meta)
        return function(moremeta)
          for i,v in pairs(moremeta) do
            meta[i] = v
          end
          metasetter(meta)
        end
      end
    end,
  }
  local parm=function(name)
    return parmlut[name]
  end
  local allParms=function()
    return parmlut
  end

  local tablelength=function(t)
    local len=0
    if type(t)=='table' then
      for i,v in pairs(t) do
        len = len+1
      end
    end
    return len
  end

  local function cppClassName(parm, fullname)
    local class = parm.meta and parm.meta.class
    if not class or class=='' then
      if parm.ui=='menu' and parm.meta.items and #parm.meta.items>0 then --enum
        class = fmt('menu_%s', escape(parm.name))
      else --struct
        class = fmt('%s_%s', parm.type, parm.name)
      end
    end
    if fullname then
      local t=parm.parent
      while t~=root do
        if not t.flat then
          class=cppClassName(t)..'::'..class
        end
        t=t.parent
      end
      class=parmsetname..'::'..class
    end
    return class
  end

  -- from parm path to variable name
  local function cppVarName(rootvar)
    return function(path)
      local field=parmlut[path]
      assert(field, fmt('cannot locate field %q', path))
      local varname = field.name
      local t=field.parent
      while t~=root do
        if not t.flat then
          varname=t.name..'.'..varname
        end
        t=t.parent
      end
      return rootvar..'.'..varname
    end
  end

  local function _genCppFields(branch, indent)
    local typeof = type
    local code=''
    indent = indent or 1
    local indentstr = string.rep(' ', indent*2)
    local emit=function(line)
      code=code..indentstr..line..'\n'
    end
    local emitf=function(line,...)
      code=code..indentstr..fmt(line,...)..'\n'
    end
    for _,v in pairs(branch.fields) do
      if not v.type or not v.name then
        dbgf('skipping %s', v.ui or v.label or v.name)
        goto bypass_field
      end
      local type = typedefs[v.type] or v.type
      local arr = type:match('%[%d+%]') or ''
      type = type:match('[%w:<>()]+')
      local default = v.meta and v.meta.default
      if v.fields then -- a branch node
        local class = cppClassName(v)
        if v.flat then
          code = code .. _genCppFields(v, indent)
          goto bypass_field
        else
          emitf('struct %s { // %s', class, v.type)
          code = code .. _genCppFields(v, indent+1)
          emit('};')
        end
        if v.type=='list' then
          type = fmt('std::vector<%s>', class)
        else --if v.type=='struct' then
          type = class
        end
      elseif v.ui=='menu' and v.meta.items and #v.meta.items>0 then
        local class = cppClassName(v)
        type = class
        emitf('enum class %s {', class)
        local nextvalue = 0
        for idx, item in ipairs(v.meta.items) do
          local label=v.meta.itemlabels and v.meta.itemlabels[idx]
          local labelcomment=label and fmt(' // label=%s', label) or ''
          local value=v.meta.itemvalues and v.meta.itemvalues[idx] or nextvalue
          emitf('  %s=%d,%s', item, value, labelcomment)
          nextvalue = value+1
        end
        emit('};')
        if default then
          default = class..'::'..default
        end
      elseif v.type=='' then
        goto bypass_field
      end
      if v.type=='string' and typeof(default)=='string' then
        default = quoteString(default)
      elseif typeof(default)=='table' then
        local tofloat = function(f)
          local s=tostring(f)
          if s:find('[.e]') then
            return s..'f'
          elseif s:match('%d+')==s then
            return s..'.0f'
          else
            return s
          end
        end
        if v.type=='color' then
          local solidalpha=''
          if v.type=='color' and v.meta and not v.meta.alpha and #default==3 then
            solidalpha=', 1.0f'
          end
          default = fmt('{%s%s}', table.concat(map(tofloat, default), ', '), solidalpha)
        else
          if v.type:find('float') then
            default = fmt('{%s}', table.concat(map(tofloat, default), ', '))
          else
            default = fmt('{%s}', table.concat(map(tostring, default), ', '))
          end
        end
      end
      if default then
        default = ' = '..tostring(default)
      else
        default = ''
      end

      local comment = {}
      local nmeta = tablelength(v.meta)
      local nthmeta = 1
      if v.ui then
        table.insert(comment, fmt('ui=%q', v.ui))
      end
      for k,m in pairs(v.meta or {}) do
        if typeof(m)=='table' or k=='default' then
          -- pass
        else
          table.insert(comment, fmt('%s=%q', k, m))
        end
      end
      if #comment>0 then
        emitf('%s %s%s%s; // %s', type, v.name, arr, default, table.concat(comment, ', '))
      else
        emitf('%s %s%s%s;', type, v.name, arr, default)
      end
      ::bypass_field::
    end
    return code
  end

  -- generate cpp structure
  local genCppStruct=function()
    local code = fmt('struct %s {\n', parmsetname)
    code = code.._genCppFields(root)
    code = code..'};\n'
    return code
  end

  local function _genImGuiInspector(rootvar, container, branch, indent, arrayidx)
    local typeof = type
    local code=''
    indent = indent or 1
    local indentstr = string.rep(' ', indent*2)
    local emit=function(line)
      code=code..indentstr..line..'\n'
    end
    local emitf=function(line,...)
      code=code..indentstr..fmt(line,...)..'\n'
    end
    local numericformat = {
      int='Int',
      int2='Int2',
      int3='Int3',
      int4='Int4',
      float='Float',
      float2='Float2',
      float3='Float3',
      float4='Float4',
    }
    for _,v in pairs(branch.fields) do
      local type = v.type
      local default = v.meta and v.meta.default
      local cannotjoin = false -- cannot join next
      local label=v.meta and v.meta.label or v.name
      local thisvar = container..'.'..v.name
      local disablewhen = v.meta and v.meta.disablewhen
      if disablewhen then
        local disableexpr = disablewhen:gsub('{([%w_.:]+)}', function(t)
          dbgf('gsub %s ..', t)
          local decoration=t:match('%w+:')
          if not decoration then
            return cppVarName(rootvar)(t)
          elseif decoration=='menu:' then
            t = t:sub(6)
            local n = t:match('::([%w_]+)')
            t = t:match('([%w_.]+)::')
            assert(parmlut[t], fmt('cannot find parm %q', t))
            return cppClassName(parmlut[t],true)..'::'..n
          elseif decoration=='length:' then
            t = t:sub(8)
            assert(parmlut[t], fmt('cannot find parm %q', t))
            assert(parmlut[t].type == 'list', fmt('%q is not a list', t))
            return t..'.size()'
          else
            assert(false, fmt('unknown decoration: %q', decoration))
          end
        end)
        emitf('ImGui::BeginDisabled(%s);', disableexpr)
      end
      if v.flat then
        thisvar = container
      end
      if arrayidx then
        emitf('std::string label_with_id_%s = %q"["+std::to_string(%s)+"]##%s";', v.name, label, arrayidx, v.name)
        label=fmt('label_with_id_%s.c_str()', v.name)
      else
        label = titleize(label)..'##'..v.name
        label=quoteString(label)
      end
      if _ == #branch.fields then
        cannotjoin = true
      end
      if v.fields then -- a branch node
        if v.type=='group' then
          local flags = v.meta and v.meta.closed and '0' or 'ImGuiTreeNodeFlags_DefaultOpen'
          emitf('if(ImGui::CollapsingHeader(%s, %s)) {', label, flags)
          code = code .. _genImGuiInspector(rootvar, thisvar, v, indent+1);
          emit('}')
        elseif v.type=='struct' then
          emitf('if(ImGui::TreeNodeEx(%s, ImGuiTreeNodeFlags_Framed)) {', label)
          code = code .. _genImGuiInspector(rootvar, thisvar, v, indent+1);
          emit('  ImGui::TreePop();')
          emit('}')
        elseif v.type=='list' then
          local countvar=fmt('list%s_cnt', v.name)
          emitf('int %s=static_cast<int>((%s).size());', countvar, thisvar)
          emitf('if (ImGui::InputInt("# " %s, &%s)) {', label, countvar)
          emitf('  %s.resize(%s);', thisvar, countvar);
          emitf('  modified.insert(%q);', v.path)
          emit ('}')
          local listidx=fmt('list%s_idx', v.name)
          emitf('for(int %s=0; %s<%s; ++%s) {', listidx, listidx, countvar, listidx)
          code = code.._genImGuiInspector(rootvar, thisvar..'['..listidx..']', v, indent+1, listidx);
          emitf('  if (%s+1<%s) ImGui::Separator();', listidx, countvar)
          emit ('}')
        else
          emit(fmt('// TODO: unknown branch type %s', v.type))
        end
        cannotjoin = true
      elseif v.ui=='menu' and v.meta.items and #v.meta.items>0 then
        local class = cppClassName(v,true)
        local labels={}
        local values={}
        for idx, item in ipairs(v.meta.items) do
          local ilabel=v.meta.itemlabels and v.meta.itemlabels[idx]
          ilabel = ilabel or titleize(item)
          ilabel = quoteString(ilabel)
          table.insert(labels, ilabel)
          table.insert(values, fmt('%s::%s', class, item))
        end
        emitf('static const char* %s_labels[]={%s};', v.name, table.concat(labels, ", "))
        emitf('static const %s %s_values[]={%s};', class, v.name, table.concat(values, ", "))
        emitf('int current_item_%s = 0;', v.name)
        emitf('for(; current_item_%s < %d; ++current_item_%s)', v.name, #values, v.name)
        emitf('  if (%s_values[current_item_%s]==(%s)) break;',
                 v.name, v.name, thisvar)
        emitf('if (ImGui::Combo(%s, &current_item_%s, %s_labels, %d)) {',
          label, v.name, v.name, #labels)
        emitf('  %s = %s_values[current_item_%s];', thisvar, v.name, v.name)
        emitf('  modified.insert(%q);', v.path)
        emit ('}')

        cannotjoin = true
      elseif v.ui=='toggle' then
        assert(v.type=='bool')
        emitf('if(ImGui::Checkbox(%s, &(%s))) modified.insert(%q);', label, thisvar, v.path)
      elseif v.ui=='button' then
        assert(v.type=='function')
        emitf('if(ImGui::Button(%s)) {', label)
        emitf('  if(%s) (%s)();', thisvar, thisvar)
        emitf('  modified.insert(%q);', v.path)
        emit ('}')
      elseif numericformat[v.type] then
        local ctl = numericformat[v.type]
        local addr = '&'..thisvar
        if typedefs[v.type] and typedefs[v.type]:find('%[%d+%]') then
          addr = fmt('(%s)', thisvar)
        else
          local convert = ''
          if v.type:find('[234]') then
            convert = fmt('reinterpret_cast<%s*>', v.type:match('%a+'))
          end
          addr = fmt('%s(&(%s))', convert, thisvar)
        end
        local min, max = v.meta and v.meta.min or 0, v.meta and v.meta.max or 1
        local minmax = ''
        if v.meta and v.meta.min and v.meta.max then
          if v.type:find('int') then
            minmax=fmt(', %d, %d', v.meta.min, v.meta.max)
          elseif v.type:find('float') then
            minmax=fmt(', %f, %f', v.meta.min, v.meta.max)
          end
        end
        local ui = v.meta and v.meta.ui or 'drag'
        if ui=='slider' or v.meta and (not v.meta.ui and v.meta.min and v.meta.max) then
          emitf('if(ImGui::Slider%s(%s, %s%s))',
            ctl, label, addr, minmax)
        elseif ui=='drag' then
          local spd=v.meta and v.meta.speed or 1.0
          emitf('if(ImGui::Drag%s(%s, %s, %ff%s))', ctl, label, addr, spd, minmax)
        else --input
          emitf('if(ImGui::Input%s(%s, %s))', ctl, label, addr)
        end
        emitf('  modified.insert(%q);', v.path)
      elseif v.type=='double' then
        emitf('if(ImGui::InputDouble(%s, &(%s)))', label, thisvar)
        emitf('  modified.insert(%q);', v.path)
      elseif v.type=='color' then
        local ctl = 'ColorEdit'
        local meta = v.meta or {}
        local alpha = true
        if meta.ui=='picker' then
          ctl = 'ColorPicker'
        end
        if meta.alpha==false then -- meta.alpha==nil implys true
          alpha = false
        end
        if alpha then
          ctl = ctl..'4'
        else
          ctl = ctl..'3'
        end
        local flags={}
        if alpha then
          flags={'AlphaBar', 'AlphaPreview', 'AlphaPreviewHalf'}
        else
          flags={'NoAlpha'}
        end
        if meta.hdr then table.insert(flags,'HDR') end
        table.insert(flags, meta.hsv and 'DisplayHSV' or 'DisplayRGB')
        table.insert(flags, (meta.float or meta.hdr) and 'Float' or 'Uint8')
        if meta.wheel then table.insert(flags,'PickerHueWheel') end
        local flagstring = table.concat(map(function(t) return 'ImGuiColorEditFlags_'..t end, flags), ' | ')
        if flagstring=='' then
          flagstring='0'
        end
        if typedefs[v.type] and typedefs[v.type]:match('float%[[34]%]') then
          emitf('if(ImGui::%s(%s, (%s), %s))', ctl, label, thisvar, flagstring)
        else
          emitf('if(ImGui::%s(%s, reinterpret_cast<float*>(&(%s)), %s))', ctl, label, thisvar, flagstring)
        end
        emitf('  modified.insert(%q);', v.path)
      elseif v.ui=='text' then
        assert(v.type=='string')
        if v.meta and v.meta.multiline then
          emitf('if(ImGui::InputTextMultiline(%s, &(%s)))', label, thisvar)
        else
          emitf('if(ImGui::InputText(%s, &(%s)))', label, thisvar)
        end
        emitf('  modified.insert(%q);', v.path)
      elseif v.ui=='label' then
        assert(not v.type or v.type=='')
        emitf('ImGui::TextUnformatted(%s);', quoteString(v.meta.label))
      elseif v.ui=='separator' then
        assert(not v.type or v.type=='')
        emit('ImGui::Separator();')
      elseif v.ui=='spacer' then
        assert(not v.type or v.type=='')
        emit('ImGui::Spacing();')
      else
        emitf('// TODO: unhandled: %s %s', v.type, v.name)
      end

      if disablewhen then
        emit('ImGui::EndDisabled();')
      end
      if v.meta and v.meta.joinnext and not cannotjoin then
        emit('ImGui::SameLine();')
      end
    end
    return code
  end

  local genImGuiInspector=function(var)
    var = var or 'parms'
    local code = fmt('bool ImGuiInspect(%s &%s, std::unordered_set<std::string>& modified) {\n', parmsetname, var)
    code = code .. '  modified.clear();\n'
    code = code .. _genImGuiInspector(var, var, root)
    code = code .. '  return !modified.empty();\n'
    code = code .. '}\n'
    return code
  end

  local f, msg=load(parmscript,'parmscript','t',safeenv)
  if not f then
    return error(msg)
  end
  local result
  result, msg = pcall(f)
  if result then
    return {
      root=root,
      parm=parm,
      allParms=allParms,
      setTypedefs=function(td) typedefs=td end,
      typedef=function(a,b) typedefs[a]=b end,
      setUseBuiltinTypes=function()
        typedefs={['function']='std::function<void()>', string='std::string', float2='float[2]', float3='float[3]', color='float[4]'}
      end,

      parmsetName=function() return parmsetname end,
      cppStruct=genCppStruct,
      imguiInspector=genImGuiInspector,
    }
  else
    return error(msg)
    --return nil
  end
end

return loadParmScript
