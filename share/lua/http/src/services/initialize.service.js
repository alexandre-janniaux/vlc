import { svgIcon } from '../components/svg-icon/svg-icon.component.js';
import store from '../store/index.js';

export const VIDEO_TYPES = [
    'asf', 'avi', 'bik', 'bin', 'divx', 'drc', 'dv', 'f4v', 'flv', 'gxf', 'iso',
    'm1v', 'm2v', 'm2t', 'm2ts', 'm4v', 'mkv', 'mov',
    'mp2', 'mp4', 'mpeg', 'mpeg1',
    'mpeg2', 'mpeg4', 'mpg', 'mts', 'mtv', 'mxf', 'mxg', 'nuv',
    'ogg', 'ogm', 'ogv', 'ogx', 'ps',
    'rec', 'rm', 'rmvb', 'rpl', 'thp', 'ts', 'txd', 'vob', 'wmv', 'xesc'
];

export const AUDIO_TYPES = [
    '3ga', 'a52', 'aac', 'ac3', 'ape', 'awb', 'dts', 'flac', 'it',
    'm4a', 'm4p', 'mka', 'mlp', 'mod', 'mp1', 'mp2', 'mp3',
    'oga', 'ogg', 'oma', 's3m', 'spx', 'thd', 'tta',
    'wav', 'wma', 'wv', 'xm'
];

export const PLAYLIST_TYPES = [
    'asx', 'b4s', 'cue', 'ifo', 'm3u', 'm3u8', 'pls', 'ram', 'rar',
    'sdp', 'vlc', 'xspf', 'zip', 'conf'
];

function vueInit() {
    Vue.use(svgIcon, {
        tagName: 'svg-icon'
    });
    return new Vue({
        el: '#app',
        data: {
            playlistItems: []
        },
        store,
        mounted() {
            this.$nextTick(() => {
                $('#openNavButton').on('click', () => {
                    if ($(window).width() <= 480 && $('#playlistNav').css('width') === '60%') {
                        this.$store.dispatch('layout/closePlaylist');
                        this.$store.dispatch('layout/openNavbar');
                    } else {
                        this.$store.dispatch('layout/openNavbar');
                    }
                });

                $('#mobilePlaylistNavButton').on('click', () => {
                    if ($(window).width() <= 480 && $('#sideNav').width() === '60%') {
                        this.$store.dispatch('layout/closeNavbar');
                        this.$store.dispatch('layout/openPlaylist');
                    } else {
                        this.$store.dispatch('layout/openPlaylist');
                    }
                });

                const container = $('#playlistNav');
                if ($(window).width() <= 480 && !container.is(e.target) && container.has(e.target).length === 0 && container.css('width') !== '0px') {
                    this.$store.dispatch('layout/closePlaylist');
                }
            });
        }
    });
}

$(() => {
    vueInit();
});
