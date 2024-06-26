const {
  program
} = require('commander');
const fs = require('fs');
const csv = require('csv-parser')
const path = require('path')

const ME = require("meta-encryptor")
const {
  Sealer,
  CSVSealer,
  ToString
} = ME;
const {
  dataHashOfSealedFile
} = ME;

program
  .description('YeeZ Privacy Data Hub')
  .option('--data-url <string>', 'data file path')
  .option('--use-publickey-file <string>', 'public key file path')
  .option('--sealed-data-url <string>', 'sealed data file path')
  .option('--output <string>', 'data meta output');

program.parse();
const options = program.opts();

if (!options.dataUrl) {
  console.log('option --data-url not specified!');
  return;
}
if (!options.usePublickeyFile) {
  console.log('option --use-publickey-file not specified!');
  return;
}
if (!options.sealedDataUrl) {
  console.log('option --sealed-data-url not specified!');
  return;
}
if (!options.output) {
  console.log('option --output not specified!');
  return;
}

//const data_lines = new nReadlines(options.dataUrl);
// TODO dian public key should specify

//DataProvider.init(data_lines);
const key_file = JSON.parse(fs.readFileSync(options.usePublickeyFile))
const key_pair = {
  private_key: key_file["private-key"],
  public_key: key_file["public-key"]
}
async function sealFile(key, src, dst) {
    let rs = fs.createReadStream(src);
    let ws = fs.createWriteStream(dst);
    if (path.extname(src) === ".csv") {
      rs.pipe(csv({
        header: false
      })).pipe(new ToString()).pipe(new CSVSealer({
        keyPair: key_pair
      })).pipe(ws)
    } else {
      rs.pipe(new Sealer({
        keyPair: key_pair
      })).pipe(ws);
    }
    await new Promise(resolve => {
      ws.on('finish', () => {
        resolve();
      });
    });
  }
  (async () => {
    await sealFile(key_file, options.dataUrl, options.sealedDataUrl)
    let output_content = '';
    output_content += ('public_key = ' + key_file['public-key'] + '\n');
    output_content += ('data_id = ' + dataHashOfSealedFile(options.sealedDataUrl).toString('hex') + '\n');
    fs.writeFileSync(options.output, output_content);
  })()
