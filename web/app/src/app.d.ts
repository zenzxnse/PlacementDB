declare global {
	namespace App {
		interface Error {
			message: string;
			code?: string;
			request_id?: string;
		}
	}
}

export {};
