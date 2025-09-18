# SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
# SPDX-License-Identifier: LicenseRef-ZrythmLicense

import re
import sys
from pathlib import Path
from lib4sbom.generator import SBOMGenerator
from lib4sbom.data.package import SBOMPackage
from lib4sbom.data.relationship import SBOMRelationship
from lib4sbom.sbom import SBOM
from lib4sbom.parser import SBOMParser

def parse_package_lock_file(lock_path):
    """Parse CPM package-lock.cmake file to extract CPMDeclarePackage dependencies"""
    dependencies = []

    try:
        with open(lock_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Error: package-lock.cmake not found at {lock_path}")
        return dependencies

    # Pattern to match CPMDeclarePackage calls
    cpm_pattern = r'CPMDeclarePackage\(([^)]+)\)'
    cpm_matches = re.findall(cpm_pattern, content, re.DOTALL)

    for match in cpm_matches:
        dep_info = {}

        # Extract NAME
        name_match = re.search(r'NAME\s+([^\s]+)', match)
        if name_match:
            dep_info['name'] = name_match.group(1)
        else:
            continue  # Skip if no NAME found

        # Extract GITHUB_REPOSITORY
        repo_match = re.search(r'GITHUB_REPOSITORY\s+([^\s]+)', match)
        if repo_match:
            dep_info['repository'] = repo_match.group(1)

        # Extract VERSION
        version_match = re.search(r'VERSION\s+([^\s]+)', match)
        if version_match:
            dep_info['version'] = version_match.group(1)

        # Extract GIT_TAG
        git_tag_match = re.search(r'GIT_TAG\s+([^\s]+)', match)
        if git_tag_match:
            dep_info['git_tag'] = git_tag_match.group(1)

        # Extract SBOM_LICENSE_CONCLUDED if available (capture quoted string)
        license_match = re.search(r'SBOM_LICENSE_CONCLUDED\s+\"([^\"]+)\"', match)
        if license_match:
            dep_info['license_concluded'] = license_match.group(1)

        # Check if package should be skipped from SBOM
        sbom_skip_match = re.search(r'SBOM_SKIP\s+([^\s]+)', match)
        if sbom_skip_match and sbom_skip_match.group(1).upper() == 'YES':
            print(f"Skipping package {dep_info['name']} due to SBOM_SKIP YES")
            continue

        dependencies.append(dep_info)

    return dependencies

def create_sbom_packages(dependencies):
    """Create SBOM packages from parsed dependencies"""
    sbom_packages = {}

    for dep in dependencies:
        package = SBOMPackage()

        # Set basic package information
        package.set_name(dep['name'])
        package.set_type('library')

        # Set version if available
        if 'version' in dep:
            package.set_version(dep['version'])

        # Set download location and supplier from repository
        if 'repository' in dep:
            repo_url = f"https://github.com/{dep['repository']}"
            package.set_downloadlocation(repo_url)

            # Extract supplier from repository (organization/user part)
            repo_parts = dep['repository'].split('/')
            if len(repo_parts) >= 1:
                supplier_org = repo_parts[0]
                # Set supplier as organization
                package.set_supplier('Organization', supplier_org)

            # Create PURL identifier
            purl = f"pkg:github/{dep['repository']}"
            if 'git_tag' in dep:
                purl += f"@{dep['git_tag']}"

            package.set_externalreference('PACKAGE-MANAGER', 'purl', purl)

        # Set license information
        if 'license_concluded' in dep:
            package.set_licenseconcluded(dep['license_concluded'])
            package.set_licensedeclared(dep['license_concluded'])
        else:
            # Default to NOASSERTION since we don't have this info
            package.set_licenseconcluded('NOASSERTION')
            package.set_licensedeclared('NOASSERTION')

        # Set copyright text
        if 'copyright_text' in dep:
            package.set_copyrighttext(dep['copyright_text'])
        else:
            package.set_copyrighttext('NOASSERTION')

        # Generate package ID
        package_id = f"SPDXRef-Package-{dep['name']}"
        package.set_id(package_id)

        sbom_packages[(dep['name'], dep.get('version', 'unknown'))] = package.get_package()

    return sbom_packages

def create_sbom_relationships(packages, qt_packages=None):
    """Create SBOM relationships between document, zrythm, and other packages"""
    relationships = []
    qt_packages = qt_packages or {}

    # Create relationship: Document DESCRIBES zrythm package
    zrythm_package_id = "SPDXRef-Package-Zrythm"
    relationship_obj = SBOMRelationship()
    relationship_obj.set_relationship('SPDXRef-DOCUMENT', 'DESCRIBES', zrythm_package_id)
    relationships.append(relationship_obj.get_relationship())

    # Create relationships: zrythm DEPENDS_ON all CPM packages
    for package_info in packages.values():
        if 'id' in package_info and package_info['id'] != zrythm_package_id:
            relationship_obj = SBOMRelationship()
            relationship_obj.set_relationship(zrythm_package_id, 'DEPENDS_ON', package_info['id'])
            relationships.append(relationship_obj.get_relationship())

    # Create relationships: zrythm DEPENDS_ON Qt packages (qtbase and qtdeclarative specifically)
    qt_package_ids_to_depend_on = ['SPDXRef-Package-qtbase', 'SPDXRef-Package-qtdeclarative']

    for qt_package_id in qt_package_ids_to_depend_on:
        if qt_package_id in qt_packages:
            relationship_obj = SBOMRelationship()
            relationship_obj.set_relationship(zrythm_package_id, 'DEPENDS_ON', qt_package_id)
            relationships.append(relationship_obj.get_relationship())

    return relationships

def parse_qt_sbom(qt_sbom_paths):
    """Parse one or more Qt SBOM files and extract packages and relationships"""
    if not qt_sbom_paths:
        return {}, []

    # Handle single file (backward compatibility)
    if isinstance(qt_sbom_paths, str):
        qt_sbom_paths = [qt_sbom_paths]

    all_qt_packages = {}
    all_qt_relationships = []
    total_packages_parsed = 0

    for qt_sbom_path in qt_sbom_paths:
        if not qt_sbom_path or not Path(qt_sbom_path).exists():
            print(f"Warning: Qt SBOM file not found at {qt_sbom_path}")
            continue

        try:
            parser = SBOMParser()
            parser.parse_file(qt_sbom_path)
            sbom_data = parser.get_sbom()

            # Extract packages and relationships
            qt_packages = sbom_data.get('packages', {})
            qt_relationships = sbom_data.get('relationships', [])

            # Filter out packages with IDs containing "-system-3rdparty-",
            # "qt-3rdparty-sources-Test", "-qt-tool-", "-qtdeclarative-qt-app-",
            # and "SPDXRef-compiler"
            filtered_qt_packages = {}
            for package_id, package_info in qt_packages.items():
                if "-system-3rdparty-" in package_id[2]:
                    pass
                elif "qt-3rdparty-sources-Test" in package_id[2]:
                    pass
                elif "-qt-tool-" in package_id[2]:
                    pass
                elif "-qt-app-" in package_id[2]:
                    pass
                elif package_id[2] == "SPDXRef-compiler":
                    pass
                else:
                    filtered_qt_packages[package_id] = package_info

            # Merge packages and relationships
            all_qt_packages.update(filtered_qt_packages)
            all_qt_relationships.extend(qt_relationships)
            total_packages_parsed += len(filtered_qt_packages)

            print(f"Parsed {len(filtered_qt_packages)} packages from {qt_sbom_path}")

        except Exception as e:
            print(f"Error parsing Qt SBOM file {qt_sbom_path}: {e}")
            continue

    print(f"Total parsed {total_packages_parsed} packages from {len(qt_sbom_paths)} Qt SBOM file(s)")
    return all_qt_packages, all_qt_relationships

def create_zrythm_package(commit_hash='master'):
    """Create SBOM package for the Zrythm project itself"""
    package = SBOMPackage()

    # Set basic package information
    package.set_name('Zrythm')
    package.set_type('application')
    package.set_version('2.0.0-DEV')

    # Set download location and supplier
    repo_url = "https://github.com/zrythm/zrythm"
    package.set_downloadlocation(repo_url)
    package.set_supplier('Organization', 'Zrythm')

    # Create PURL identifier with git tag/commit
    purl = f"pkg:github/zrythm/zrythm@{commit_hash}"
    package.set_externalreference('PACKAGE-MANAGER', 'purl', purl)

    # Set license information (from REUSE.toml or similar)
    package.set_licenseconcluded('AGPL-3.0-only')
    package.set_licensedeclared('AGPL-3.0-only')

    # Set copyright text
    package.set_copyrighttext('© 2025 Alexandros Theodotou <alex@zrythm.org>')

    # Generate package ID
    package_id = "SPDXRef-Package-Zrythm"
    package.set_id(package_id)

    return package.get_package()

def generate_sbom(project_name, lock_path, output_file=None, qt_sbom_paths=None, zrythm_commit='master'):
    """Generate SBOM from CPM package lock dependencies and optionally include Qt SBOM(s)"""

    # Parse package lock file for dependencies
    dependencies = parse_package_lock_file(lock_path)

    if not dependencies:
        print("No CPM dependencies found in package-lock.cmake")
        return

    print(f"Found {len(dependencies)} CPM dependencies")

    # Create SBOM packages
    packages = create_sbom_packages(dependencies)

    # Add zrythm project itself as a package
    zrythm_package = create_zrythm_package(zrythm_commit)
    packages[('Zrythm', '2.0.0-DEV')] = zrythm_package

    # Parse Qt SBOM if provided
    qt_packages, qt_relationships = parse_qt_sbom(qt_sbom_paths)

    # Create SBOM relationships (pass qt_packages for dependency relationships)
    relationships = create_sbom_relationships(packages, qt_packages)

    # Create SBOM object and add packages/relationships
    sbom = SBOM()

    # Add CPM packages to SBOM
    sbom.add_packages(packages)

    # Add Qt packages to SBOM
    if qt_packages:
        sbom.add_packages(packages | qt_packages)

    # Add CPM relationships to SBOM
    sbom.add_relationships(relationships)

    # Add Qt relationships to SBOM
    if qt_relationships:
        sbom.add_relationships(qt_relationships)

    # Note: Qt dependencies are now handled in create_sbom_relationships function

    # Create SBOM generator
    generator = SBOMGenerator(
        validate_license=True,
        sbom_type="spdx",
        format="json",
        application="zrythm-sbom-generator",
        version="0.1",
    )

    # Generate SBOM using the SBOM object's data
    generator.generate(project_name, sbom.get_sbom(), filename=output_file, send_to_output=bool(output_file))

    if output_file:
        print(f"SBOM generated successfully: {output_file}")
    else:
        print("SBOM generated successfully (output to console)")

def main():
    """Main function"""
    # Default paths
    project_root = Path(__file__).parent.parent
    lock_path = project_root / "package-lock.cmake"
    output_file = project_root / "sbom.spdx.json"

    # Parse command line arguments
    import argparse
    parser = argparse.ArgumentParser(description='Generate SBOM from CPM dependencies in package-lock.cmake')
    parser.add_argument('--lock', type=str, default=str(lock_path),
                       help='Path to package-lock.cmake file')
    parser.add_argument('--output', type=str, default=str(output_file),
                       help='Output file path for SBOM')
    parser.add_argument('--project', type=str, default="Zrythm",
                        help='Project name for SBOM')
    parser.add_argument('--qt-sbom', type=str, default=None, nargs='+',
                       help='Path to Qt SBOM file(s) (SPDX tag-value format). Can specify multiple files.')
    parser.add_argument('--zrythm-commit', type=str, default='master',
                       help='Zrythm commit hash or tag to use in PURL (default: master)')

    args = parser.parse_args()

    # Generate SBOM
    try:
        generate_sbom(args.project, args.lock, args.output, args.qt_sbom, args.zrythm_commit)
    except Exception as e:
        print(f"Error generating SBOM: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
