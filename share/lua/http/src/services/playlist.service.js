export default {
    sendPlaylist(params) {
        return $.ajax({
            url: 'requests/playlist.json',
            data: params
        })
        .then((data) => {
            const jsonData = JSON.parse(data);
            return jsonData;
        });
    },
    sendPlaylistStatus(params) {
        return $.ajax({
            url: 'requests/status.json',
            data: params
        })
        .then((data) => {
            const jsonData = JSON.parse(data);
            return jsonData;
        });
    },
    fetchPlaylist() {
        return this.sendPlaylist();
    },
    addItem(src) {
        return this.sendPlaylistStatus(`command=in_enqueue&input=${src}`);
    },
    removeItem(id) {
        return this.sendPlaylistStatus(`command=pl_delete&id=${id}`);
    }
};
