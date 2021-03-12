const fs = require('fs');
const path = require('path');
const { Octokit } = require('@octokit/rest');

async function main() {
    // Need a github oauth token in env
    const githubToken = process.env.GH_TOKEN;
    if (!githubToken) {
        throw new Error('Publishing requires the environment variable GH_TOKEN to be set!');
    }    

    const github = new Octokit({
        auth: `token ${githubToken}`
    });
    const packageJSON = require('../package.json');
    // TODO: Parse the owner/repo from packageJSON.repository.url
    const owner = 'appcelerator';
    const repo = 'node-ios-device';

    const release = {
        owner,
        repo,
        tag_name: `v${packageJSON.version}`,
        target_commitish: '1_X', // TODO: How to get branch name?
        name: packageJSON.version,
    };

    // Update release if it already exists
    let uploadUrl;
    let releaseId;
    let url;
    let alreadyExists = false;
    try {
        const result = await github.repos.getReleaseByTag({ owner, repo, tag: release.tag_name });
        alreadyExists = true;
        uploadUrl = result.data.upload_url;
        releaseId = result.data.release_id;
        url = result.data.html_url;
    } catch (error) {
        // doesn't exist yet!
        // We'll create a draft release, append the assets to it, and then publish it.
        const draftRelease = { ...release, draft: true };
        const result = await github.repos.createRelease(draftRelease);
        uploadUrl = result.data.upload_url;
        releaseId = result.data.release_id;
    }

    // Append assets to the release
    const BINDING_DIR = path.join(__dirname, `../build/stage/${owner}/${repo}/releases/download/${release.tag_name}`)
    const tarballs = await fs.promises.readdir(BINDING_DIR);
    let updated = [];

    await Promise.all(
        tarballs.map(async (tarball) => {
            const filePath = path.join(BINDING_DIR, tarball);

            const fileName = path.basename(filePath);
            const upload = {
                url: uploadUrl,
                owner,
                repo,
                data: await fs.promises.readFile(filePath),
                name: fileName
            };

            console.debug('file path:', filePath);
            console.debug('file name:', fileName);

            try {
                const {
                    data: { browser_download_url: downloadUrl },
                } = await github.repos.uploadReleaseAsset(upload);
                console.log('Published file', downloadUrl);
                updated.push(filePath);
            } catch (err) {                
                if (err.errors && err.errors[0].code === 'already_exists') {
                    console.warn(`Did not publish ${filePath} as it already exists`);
                } else {
                    console.error('Failed to publish file ', filePath);
                }
            }
        })
    );

    if (!alreadyExists) {
        const {
            data: { html_url: url },
        } = await github.repos.updateRelease({ owner, repo, release_id: releaseId, draft: false });

       console.log('Published GitHub release: ', url);
    } else if (updated.length > 0) {
        console.log(`Updated Assets for GitHub release ${url}:\n- ${updated.join('\n- ')}`);
    } else {
        console.warn(`Existing GitHub release ${url} not updated, all assets existed`);
    }
}

main()
    .then(() => process.exit(0))
    .catch(err => {
        console.error(err);
        process.exit(1);
});