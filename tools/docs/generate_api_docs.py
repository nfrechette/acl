#!/usr/bin/env python3

# Generates the Markdown pages of the API reference.
# The script runs Doxygen with tools/docs/Doxyfile, then it converts the Doxygen XML to one
# Markdown page for each public header. MkDocs then makes the site from docs/ (see mkdocs.yml).
#
# Usage, from any directory: python3 tools/docs/generate_api_docs.py
# Doxygen 1.18 must be on the PATH.

import os
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT_DIR = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
DOXYFILE = os.path.join('tools', 'docs', 'Doxyfile')
DOXYGEN_OUTPUT_DIR = os.path.join('build', 'docs', 'doxygen')
API_OUTPUT_DIR = os.path.join('docs', 'api')
ACL_NAMESPACE_PREFIX = 'acl::'

# The nested namespaces that hold implementation details. Doxyfile excludes them too.
IMPL_NAMESPACE_PATTERN = re.compile(r'(^|::)\w*_impl(::|$)')

# The folders of the public headers, in the order of the index page
FOLDER_TITLES = [
	('', 'Configuration and version'),
	('core', 'Core'),
	('compression', 'Compression'),
	('decompression', 'Decompression'),
	('decompression/database', 'Database'),
	('io', 'Clip files'),
	('math', 'Math'),
]

def run_doxygen():
	os.makedirs(os.path.join(ROOT_DIR, DOXYGEN_OUTPUT_DIR), exist_ok=True)

	# Doxyfile sets WARN_AS_ERROR: a public symbol without a comment stops Doxygen with an error
	result = subprocess.run(['doxygen', DOXYFILE], cwd=ROOT_DIR, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
	for line in result.stdout.splitlines():
		if line.strip():
			print(line)

	if result.returncode != 0:
		print('Doxygen failed with exit code {}'.format(result.returncode))
		sys.exit(result.returncode)

def text_of(node):
	return '' if node is None else ''.join(node.itertext())

def escape_markdown(text):
	return text.replace('&', '&amp;').replace('<', '&lt;')

def slugify(text):
	# The same function as the toc extension of Python-Markdown
	text = re.sub(r'[^\w\s-]', '', text).strip().lower()
	return re.sub(r'[-\s]+', '-', text)

class DescriptionWriter:
	def __init__(self, links):
		self.links = links

	def write(self, node):
		if node is None:
			return ''
		return self.children(node).strip()

	def children(self, node):
		result = escape_markdown(node.text or '')
		for child in node:
			result += self.element(child)
			result += escape_markdown(child.tail or '')
		return result

	def element(self, node):
		tag = node.tag
		if tag == 'para':
			return '\n\n' + self.children(node).strip() + '\n\n'
		if tag == 'computeroutput':
			return '`' + text_of(node) + '`'
		if tag == 'emphasis':
			return '*' + self.children(node) + '*'
		if tag == 'bold':
			return '**' + self.children(node) + '**'
		if tag == 'ulink':
			return '[' + self.children(node) + '](' + node.get('url') + ')'
		if tag == 'ref':
			link = self.links.get(node.get('refid'))
			text = '`' + text_of(node) + '`'
			return '[' + text + '](' + link + ')' if link else text
		if tag == 'linebreak':
			return '<br>'
		if tag in ('itemizedlist', 'orderedlist'):
			marker = '-' if tag == 'itemizedlist' else '1.'
			items = [marker + ' ' + self.children(item).strip().replace('\n', '\n    ') for item in node.findall('listitem')]
			return '\n\n' + '\n'.join(items) + '\n\n'
		if tag == 'programlisting':
			lines = [self.code_line(line) for line in node.findall('codeline')]
			return '\n\n```cpp\n' + '\n'.join(lines) + '\n```\n\n'
		if tag == 'simplesect':
			title = {'return': 'Returns', 'note': 'Note', 'warning': 'Warning', 'see': 'See also'}.get(node.get('kind'), node.get('kind').capitalize())
			kind = 'warning' if node.get('kind') == 'warning' else 'note'
			return self.admonition(kind, title, self.children(node))
		if tag == 'xrefsect':
			title = node.findtext('xreftitle')
			kind = 'warning' if title == 'Deprecated' else 'note'
			return self.admonition(kind, title, self.children(node.find('xrefdescription')))
		if tag == 'parameterlist':
			items = []
			for item in node.findall('parameteritem'):
				names = ', '.join('`' + text_of(name) + '`' for name in item.iter('parametername'))
				items.append('- ' + names + ': ' + self.children(item.find('parameterdescription')).strip())
			return '\n\n**Parameters**\n\n' + '\n'.join(items) + '\n\n'
		return self.children(node)

	def code_line(self, node):
		text = ''
		for part in node.iter():
			if part.tag == 'sp':
				text += ' '
			elif part.text and part.tag != 'codeline':
				text += part.text
			if part.tag != 'codeline' and part.tail:
				text += part.tail
		return text

	def admonition(self, kind, title, body):
		body = re.sub(r'\n{3,}', '\n\n', body.strip())
		return '\n\n!!! {} "{}"\n\n'.format(kind, title) + '\n'.join('    ' + line if line else '' for line in body.split('\n')) + '\n\n'

def description_node_text(node):
	# The text of a description without the 'Deprecated' section that Doxygen adds for [[deprecated]]
	if node is None:
		return ''
	text = node.text or ''
	for child in node:
		if child.tag == 'para':
			text += (child.text or '') + ''.join(('' if part.tag == 'xrefsect' else text_of(part)) + (part.tail or '') for part in child)
		else:
			text += text_of(child)
		text += child.tail or ''
	return text

def has_description(node):
	return any(description_node_text(node.find(tag)).strip() for tag in ('briefdescription', 'detaileddescription'))

def page_name_of_header(header_file):
	# includes/acl/compression/compress.h -> compression_compress
	return os.path.splitext(os.path.relpath(header_file, 'includes/acl'))[0].replace('/', '_')

def is_public_member(member):
	return member.get('prot') == 'public'

class Entry:
	def __init__(self, kind, name, node, location, section=None):
		self.kind = kind
		self.name = name
		self.node = node
		self.file = location.get('file')
		self.line = int(location.get('line'))
		self.section = section
		self.members = []

def collect_entries(xml_dir):
	index = ET.parse(os.path.join(xml_dir, 'index.xml')).getroot()

	entries = []
	file_descriptions = {}
	for compound in index.findall('compound'):
		kind = compound.get('kind')
		name = compound.findtext('name')
		if kind not in ('namespace', 'struct', 'class', 'union', 'file'):
			continue
		if kind != 'file' and not (name == 'acl' or name.startswith(ACL_NAMESPACE_PREFIX)):
			continue
		if IMPL_NAMESPACE_PATTERN.search(name):
			continue

		node = ET.parse(os.path.join(xml_dir, compound.get('refid') + '.xml')).getroot().find('compounddef')

		if kind == 'namespace':
			prefix = name[len(ACL_NAMESPACE_PREFIX):] + '::' if name != 'acl' else ''
			for section in node.iter('sectiondef'):
				for member in section.findall('memberdef'):
					if member.get('kind') in ('function', 'typedef', 'enum', 'variable') and has_description(member):
						entries.append(Entry(member.get('kind'), prefix + member.findtext('name'), member, member.find('location'), section.findtext('header')))
		elif kind in ('struct', 'class', 'union'):
			if not has_description(node):
				continue
			entry = Entry(kind, name[len(ACL_NAMESPACE_PREFIX):].replace(' ', ''), node, node.find('location'))
			for section in node.findall('sectiondef'):
				for member in section.findall('memberdef'):
					if is_public_member(member) and member.get('kind') in ('function', 'typedef', 'enum', 'variable') and has_description(member):
						entry.members.append((section.findtext('header'), member))
			entry.members.sort(key=lambda item: int(item[1].find('location').get('line')))
			entries.append(entry)
		else:
			location = node.find('location')
			if location is None:
				continue
			if has_description(node):
				file_descriptions[location.get('file')] = node
			for section in node.iter('sectiondef'):
				for member in section.findall('memberdef'):
					if member.get('kind') == 'define' and has_description(member):
						entries.append(Entry('define', member.findtext('name'), member, member.find('location')))

	entries.sort(key=lambda entry: (entry.file, entry.line))
	return entries, file_descriptions

DEPRECATED_PATTERN = re.compile(r'\[\[deprecated(\(.*?\))?\]\]\s*')

class SignatureWriter:
	def clean(self, text):
		text = re.sub(r'\s+', ' ', text).strip()

		# The deprecation is in the description
		text = DEPRECATED_PATTERN.sub('', text)

		# rtm::vector4f_arg0 -> rtm::vector4f
		text = re.sub(r'\b(\w+)_arg\d+\b', r'\1', text)

		# Doxygen writes 'const float *input', ACL writes 'const float* input'
		text = re.sub(r'([\w>]) ([*&]+)(\w)', r'\1\2 \3', text)
		text = re.sub(r'([\w>]) ([*&]+)([,)]|$)', r'\1\2\3', text)
		text = re.sub(r'(\w) ([*&]+\.\.\.)', r'\1\2', text)
		text = text.replace('< ', '<').replace(' >', '>')
		return text

	def template_line(self, node):
		template_params = node.find('templateparamlist')
		if template_params is None:
			return ''
		params = []
		for param in template_params.findall('param'):
			param_text = text_of(param.find('type'))
			if param.findtext('declname'):
				param_text += ' ' + param.findtext('declname')
			if param.find('defval') is not None:
				param_text += ' = ' + text_of(param.find('defval'))
			params.append(self.clean(param_text))
		return 'template<' + ', '.join(params) + '>\n'

	def function(self, node, name):
		text = self.template_line(node)
		if node.get('static') == 'yes':
			text += 'static '
		if node.get('constexpr') == 'yes':
			text += 'constexpr '
		if node.get('explicit') == 'yes':
			text += 'explicit '
		if node.get('virt') in ('virtual', 'pure-virtual'):
			text += 'virtual '
		return_type = self.clean(text_of(node.find('type')))
		# Doxygen keeps 'static' and 'constexpr' in the type of some functions
		return_type = re.sub(r'^(static |constexpr |inline )+', '', return_type)
		if return_type:
			text += return_type + ' '
		# Doxygen writes 'int value=0', ACL writes 'int value = 0'
		args = re.sub(r'(?<=[\w\])])=(?!=)', ' = ', self.clean(node.findtext('argsstring')))
		text += name + args
		return text

	def typedef(self, node, name):
		return self.template_line(node) + 'using {} = {};'.format(name, self.clean(text_of(node.find('type'))))

	def enum(self, node, name):
		header = 'enum class ' + name if node.get('strong') == 'yes' else 'enum ' + name
		underlying = self.clean(text_of(node.find('type')))
		if underlying:
			header += ' : ' + underlying
		lines = [header, '{']
		for value in node.findall('enumvalue'):
			initializer = text_of(value.find('initializer')).strip()
			comment = re.sub(r'\s+', ' ', description_node_text(value.find('briefdescription')) + ' ' + description_node_text(value.find('detaileddescription'))).strip()
			comment = ' // ' + comment if comment else ''
			lines.append('\t' + value.findtext('name') + (' ' + initializer if initializer else '') + ',' + comment)
		lines.append('};')
		return '\n'.join(lines)

	def variable(self, node, name):
		text = self.clean(text_of(node.find('type'))) + ' ' + name + self.clean(node.findtext('argsstring') or '')
		initializer = text_of(node.find('initializer')).strip()
		if initializer:
			text += ' ' + initializer if initializer.startswith('=') else ' = ' + initializer
		return text + ';'

	def compound(self, entry):
		node = entry.node
		lines = [self.template_line(node).rstrip('\n')] if node.find('templateparamlist') is not None else []
		if '<' in entry.name and not lines:
			lines.append('template<>')

		keyword = 'struct' if entry.kind == 'struct' else entry.kind
		header = keyword + ' ' + entry.name.replace(',', ', ')
		if node.get('final') == 'yes':
			header += ' final'
		bases = [('public ' if base.get('prot') == 'public' else base.get('prot') + ' ') + self.clean(text_of(base)) for base in node.findall('basecompoundref')]
		if bases:
			header += ' : ' + ', '.join(bases)
		lines.append(header)
		return '\n'.join(line for line in lines if line)

	def define(self, node, name):
		# A function-like macro shows its parameters, and an object-like macro shows its value
		if node.find('param') is not None:
			params = [param.findtext('defname') for param in node.findall('param')]
			return '#define {}({})'.format(name, ', '.join(params))
		return '#define {} {}'.format(name, text_of(node.find('initializer')).strip())

	def member(self, node, name):
		kind = node.get('kind')
		if kind == 'function':
			return self.function(node, name)
		if kind == 'typedef':
			return self.typedef(node, name)
		if kind == 'enum':
			return self.enum(node, name)
		if kind == 'define':
			return self.define(node, name)
		return self.variable(node, name)

def describe(writer, node):
	description = writer.write(node.find('briefdescription')) + '\n\n' + writer.write(node.find('detaileddescription'))
	return re.sub(r'\n{3,}', '\n\n', description).strip()

def write_code_groups(lines, items):
	# items: a list of (code, description). Consecutive overloads with the same description share one code block.
	groups = []
	for code, description in items:
		if groups and groups[-1][1] == description:
			groups[-1][0].append(code)
		else:
			groups.append(([code], description))

	for codes, description in groups:
		# A template line applies to one signature only, thus a blank line separates the signatures
		separator = '\n\n' if any('\n' in code for code in codes) else '\n'
		lines += ['```cpp', separator.join(codes), '```', '']
		if description:
			lines += [description, '']

def heading_of(name):
	return '`{}`'.format(name) if re.search(r'[^\w]', name) else name

def write_compound(lines, entry, writer, signature_writer):
	lines += ['```cpp', signature_writer.compound(entry), '```', '']
	description = describe(writer, entry.node)
	if description:
		lines += [description, '']

	# The overloads of a member function have one heading, at the position of the first overload
	names = []
	for _, member in entry.members:
		name = member.findtext('name')
		if name not in names:
			names.append(name)

	for name in names:
		lines += ['#### {}'.format(heading_of(entry.name + '::' + name)), '']
		items = []
		for _, member in entry.members:
			if member.findtext('name') == name:
				items.append((signature_writer.member(member, name), describe(writer, member)))
		write_code_groups(lines, items)

def write_page(output_dir, page, include_path, entries, file_description, writer, signature_writer):
	lines = ['# {}'.format(include_path), '', '```cpp', '#include "{}"'.format(include_path), '```', '']
	if file_description is not None:
		text = describe(writer, file_description)
		if text:
			lines += [text, '']

	# Sections in the order of the header. Entries without a section go first.
	sections = []
	for entry in entries:
		if entry.section not in sections:
			sections.append(entry.section)
	sections.sort(key=lambda section: section is not None)

	for section in sections:
		if section:
			lines += ['## {}'.format(section), '']

		# The overloads of a function have one heading, at the position of the first overload
		names = []
		for entry in entries:
			if entry.section == section and entry.name not in names:
				names.append(entry.name)

		for name in names:
			lines += ['### {}'.format(heading_of(name)), '']

			items = []
			for entry in entries:
				if entry.section != section or entry.name != name:
					continue
				if entry.kind in ('struct', 'class', 'union'):
					write_code_groups(lines, items)
					items = []
					write_compound(lines, entry, writer, signature_writer)
				else:
					items.append((signature_writer.member(entry.node, entry.name), describe(writer, entry.node)))
			write_code_groups(lines, items)

	with open(os.path.join(output_dir, page + '.md'), 'w') as f:
		f.write('\n'.join(lines))

def write_index(output_dir, pages):
	lines = ['# API reference', '', 'Each page lists the API of one public header.', '']
	for folder, title in FOLDER_TITLES:
		folder_pages = [page for page in pages if os.path.dirname(page[1])[len('acl/'):] == folder or (not folder and os.path.dirname(page[1]) == 'acl')]
		if not folder_pages:
			continue
		lines += ['## {}'.format(title), '', '| Header | Description |', '| --- | --- |']
		for page, include_path, brief in sorted(folder_pages, key=lambda page: page[1]):
			lines.append('| [`{}`]({}.md) | {} |'.format(include_path, page, brief.replace('|', '\\|')))
		lines.append('')

	with open(os.path.join(output_dir, 'index.md'), 'w') as f:
		f.write('\n'.join(lines))

def main():
	run_doxygen()

	signature_writer = SignatureWriter()
	entries, file_descriptions = collect_entries(os.path.join(ROOT_DIR, DOXYGEN_OUTPUT_DIR, 'xml'))

	# The page of each entry
	page_entries = {}
	include_paths = {}
	page_files = {}
	for entry in entries:
		if entry.file.startswith('includes/acl/') and '/impl/' not in entry.file:
			page = page_name_of_header(entry.file)
		else:
			print('No page for {} in {}: Doxygen must read the public headers only'.format(entry.name, entry.file))
			sys.exit(1)

		include_paths[page] = entry.file[len('includes/'):]
		page_files[page] = entry.file
		page_entries.setdefault(page, []).append(entry)

	# Each entry links to its heading. A member links to the heading of its name in its class.
	links = {}
	for page, page_entry_list in sorted(page_entries.items()):
		for entry in page_entry_list:
			links.setdefault(entry.node.get('id'), '{}.md#{}'.format(page, slugify(entry.name)))
			for _, member in entry.members:
				links.setdefault(member.get('id'), '{}.md#{}'.format(page, slugify(entry.name + '::' + member.findtext('name'))))

	# The output folder holds generated pages only
	output_dir = os.path.join(ROOT_DIR, API_OUTPUT_DIR)
	if os.path.exists(output_dir):
		shutil.rmtree(output_dir)
	os.makedirs(output_dir)

	writer = DescriptionWriter(links)
	pages = []
	for page, page_entry_list in sorted(page_entries.items()):
		write_page(output_dir, page, include_paths[page], page_entry_list, file_descriptions.get(page_files.get(page)), writer, signature_writer)
		# The index shows the brief description of each header: a '/// \\file' comment in the header
		file_description = file_descriptions.get(page_files.get(page))
		brief = ''
		if file_description is not None:
			# Without a brief description, the first paragraph of the detailed description
			brief = writer.write(file_description.find('briefdescription')) or writer.write(file_description.find('detaileddescription')).split('\n\n')[0]
			brief = re.sub(r'\s+', ' ', brief)
		if not brief:
			print('No brief description for {}: add a /// \\file comment to the header'.format(include_paths[page]))
			sys.exit(1)
		pages.append((page, include_paths[page], brief))

	write_index(output_dir, pages)
	print('Wrote {} pages in {}'.format(len(pages) + 1, API_OUTPUT_DIR))

if __name__ == '__main__':
	main()
